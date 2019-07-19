Summary: 

The PointCloudCenter program presents a algorithm for locating a center of a point cloud as applied to the cloud of random points inside a unit cube.

Definitions:

Point cloud - an arbitrary set of 3D points, typically represented by their Cartesian coordinates. 
Distance between two sets of points - a sum of pairwise distances between their points.
Point cloud center - a point that is closest to the point cloud.
Unit cube - a cube with opposing vertexes (0,0,0) and (1,1,1) with 3 edges along coordinate axis.
Distance beween a cube and a cloud - distance between its vertexes and the cloud

The algorithm:

We begin with the unit cube as the current closest. With each iteration, we subdivide the 
current closest cube into 8 subcubes, find a subcube that is the closest to the cloud
and declare it the next current closest. After a desired number of iterations, the center of the current closest cube is 
deemed as the center of the cloud.

By far the lengthiest part of the algorithm is a calculation of a distance between a cube vertex and a cloud.
Multi-threading is used to parallelize these calculations.


