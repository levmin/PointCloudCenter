#include <iostream>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <assert.h>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <algorithm>

using namespace std;

constexpr size_t CLOUD_SIZE = 10000000;
constexpr int    ITERATIONS = 12;
constexpr unsigned    MAX_NUM_THREADS = 4;

const char* testCloudFileName = "TestCloud.bin";

using Vector3D=double[3];

inline void setVec(Vector3D& v, double x, double y, double z)
{
   v[0] = x;
   v[1] = y;
   v[2] = z;
}

inline double getDistance (const Vector3D& coord, const vector<Vector3D>& cloud)
{
   double distance = 0;
   for (auto& p : cloud)
      distance += hypot(p[0] - coord[0], p[1] - coord[1], p[2] - coord[2]);
   return distance;
}

struct VertexData
{
   Vector3D coord = { 0,0,0 };
   bool processed = false;
   double distance = 0;
   inline void calcDistance (const vector<Vector3D>& cloud)
   {
      distance = getDistance(coord, cloud);
      processed = true;
   }
   VertexData(const VertexData& data) = delete;
   VertexData() = default;
   VertexData& operator=(const VertexData& data)
   {
      memcpy_s(coord, sizeof(coord), data.coord, sizeof(data.coord));
      processed = data.processed;
      distance = data.distance;
      return *this;
   }
};

class DistanceFinder
{
public:
   DistanceFinder(const vector<Vector3D>* _cloud) { cloud = _cloud; }
   void addVertexData(VertexData* pData)
   {
      lock_guard<std::mutex> guard(m);
      VertexData2 data{ pData, false };
      m_data.push_back(data);
   }
   void calcDistances()
   {
      int numThreads = min(std::thread::hardware_concurrency(), MAX_NUM_THREADS);
      std::vector<std::thread> threads;
      while (numThreads--)
         threads.emplace_back(
             ThreadProc
         );
      for (auto& t : threads)
         t.join();
      m_data.clear();
   }

private:

   struct VertexData2
   {
      VertexData* pData=NULL;
      bool taken=false;
   };

   static void ThreadProc()
   {
      while (true)
      {
         VertexData2 * pData2 = NULL;
         {
            lock_guard<std::mutex> guard(m);
            for (auto& data : m_data)
            {
               if (!data.taken)
               {
                  data.taken = true;
                  pData2 = &data;
                  break;
               }
            }
         }
         if (!pData2)
            return;
         pData2->pData->calcDistance(*cloud);
      }
   }
   static std::mutex m;
   static vector<VertexData2> m_data;
   static const vector<Vector3D>* cloud;
};

std::mutex DistanceFinder::m;
vector<DistanceFinder::VertexData2>  DistanceFinder::m_data;
const vector<Vector3D>* DistanceFinder::cloud;

int main()
{
   vector<Vector3D> cloud(CLOUD_SIZE);
   for (auto& v : cloud)
      setVec(v, 2, 2, 2);
   if (filesystem::exists(testCloudFileName))
   {
      cout << "Cloud exists\n";
      FILE* in = NULL;
      fopen_s(&in,testCloudFileName,"rb");
      fread((char*)& (cloud[0][0]), sizeof(Vector3D) , cloud.size(),in);
      fclose(in);
      //sanity check
      for (auto& v : cloud)
         for (int i = 0; i < 3; i++)
         {
            if (isnan(v[i]) || v[i] < 0 || v[i]>1)
            {
               cout << "Sanity check failed\n";
               return -1;
            }
         }
      cout << "Sanity check succeeded\n";
   }
   else
   {
      cout << "Cloud does not exist. Creating it.\n";
      FILE* out = NULL;
      fopen_s(&out, testCloudFileName, "wb");
      for (auto& v : cloud)
         for (int i = 0; i < 3; i++)
            v[i] = ((double)rand())/RAND_MAX;
      fwrite((const char*)&cloud[0][0], sizeof(Vector3D) , cloud.size(),out);
      fclose(out);
   }
   auto start = chrono::steady_clock::now();

   VertexData vertexes[3][3][3];
   auto& currentLowerLowerLeft = vertexes[0][0][0].coord;
   double cubeHalfSideLength = 1./2.;
   DistanceFinder finder(&cloud);
   //process the cube vertexes
   for (int x=0;x<3;x+=2)
      for (int y = 0; y < 3; y += 2)
         for (int z = 0; z < 3; z += 2)
         {
            auto& v = vertexes[x][y][z];
            setVec(v.coord,x * cubeHalfSideLength, y * cubeHalfSideLength, z * cubeHalfSideLength );
            finder.addVertexData(&v);
         }
   finder.calcDistances();
   //let's do a few iterations, every time dividing the cube into 8 subcubes
   for (int i = 0; i < ITERATIONS; i++)
   {
      //fill the remaining vertexes
      for (int x = 0; x < 3; x++)
         for (int y = 0; y < 3; y ++)
            for (int z = 0; z < 3; z++)
            {
               auto& v = vertexes[x][y][z];
               if (!v.processed)
               {
                  setVec(v.coord, currentLowerLowerLeft[0]+ x * cubeHalfSideLength, currentLowerLowerLeft[1] + y * cubeHalfSideLength, currentLowerLowerLeft[2] + z * cubeHalfSideLength);
                  finder.addVertexData(&v);
               }
            }
      finder.calcDistances();
      //find the next candidate cube among 8 subcubes
      double minDistance = std::numeric_limits<double>::max();
      int minX = 0;
      int minY = 0;
      int minZ = 0;
      for (int x = 0; x < 2; x++)
         for (int y = 0; y < 2; y++)
            for (int z = 0; z < 2; z++)
            {
               double distance = 0;
               for (int x2 = 0; x2 < 2; x2++)
                  for (int y2 = 0; y2 < 2; y2++)
                     for (int z2 = 0; z2 < 2; z2++)
                        distance += vertexes[x + x2][y + y2][z + z2].distance;
               if (distance < minDistance)
               {
                  minDistance = distance;
                  minX = x;
                  minY = y;
                  minZ = z;
               }
            }
      //transition to the next step - make the subcube the new cube
      VertexData newVertexes[3][3][3];
      for (int x = 0; x < 3; x += 2)
         for (int y = 0; y < 3; y += 2)
            for (int z = 0; z < 3; z += 2)
               newVertexes[x][y][z] = vertexes[minX + x / 2][minY + y / 2][minZ + z / 2];
      //copy the new vertexes onto the old vertexes
      for (int x = 0; x < 3; x++)
         for (int y = 0; y < 3; y++)
            for (int z = 0; z < 3; z++)
               vertexes[x][y][z] = newVertexes[x][y][z];
      //divide cubeHalfSideLength  in half
      cubeHalfSideLength /= 2;
   }
   //find the cube center
   const auto& coord0 = vertexes[0][0][0].coord;
   const auto& coord1 = vertexes[2][2][2].coord;
   Vector3D center{ (coord0[0] + coord1[0]) / 2,(coord0[1] + coord1[1]) / 2, (coord0[2] + coord1[2]) / 2 };
   auto end = chrono::steady_clock::now();
   
   cout << "Elapsed time in milliseconds : "
      << chrono::duration_cast<chrono::milliseconds>(end - start).count()
      << " ms" << endl;

   cout << "Cloud center is (" << center[0] << "," << center[1] << "," << center[2] << ")\n";
   return 0;
}