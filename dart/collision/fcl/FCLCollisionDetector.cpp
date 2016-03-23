/*
 * Copyright (c) 2011-2015, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Chen Tang <ctang40@gatech.edu>,
 *            Jeongseok Lee <jslee02@gmail.com>
 *
 * Georgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "dart/collision/fcl/FCLCollisionDetector.h"

#include <assimp/scene.h>

#include <fcl/collision.h>
#include <fcl/collision_object.h>
#include <fcl/collision_data.h>
#include <fcl/BVH/BVH_model.h>
#include <fcl/broadphase/broadphase.h>
#include <fcl/shape/geometric_shapes.h>
#include <fcl/shape/geometric_shape_to_BVH_model.h>

#include "dart/common/Console.h"
#include "dart/collision/CollisionObject.h"
#include "dart/collision/fcl/FCLTypes.h"
#include "dart/collision/fcl/FCLCollisionObject.h"
#include "dart/collision/fcl/FCLCollisionGroup.h"
#include "dart/dynamics/ShapeFrame.h"
#include "dart/dynamics/Shape.h"
#include "dart/dynamics/BoxShape.h"
#include "dart/dynamics/EllipsoidShape.h"
#include "dart/dynamics/CylinderShape.h"
#include "dart/dynamics/PlaneShape.h"
#include "dart/dynamics/Shape.h"
#include "dart/dynamics/MeshShape.h"
#include "dart/dynamics/SoftMeshShape.h"

namespace dart {
namespace collision {

namespace {

bool collisionCallback(fcl::CollisionObject* o1,
                       fcl::CollisionObject* o2,
                       void* cdata);

void postProcess(const fcl::CollisionResult& fclResult,
                 fcl::CollisionObject* o1,
                 fcl::CollisionObject* o2,
                 Result& result);

void convertOption(const Option& fclOption, fcl::CollisionRequest& request);

Contact convertContact(const fcl::Contact& fclContact,
                       fcl::CollisionObject* o1,
                       fcl::CollisionObject* o2);

/// Collision data stores the collision request and the result given by
/// collision algorithm.
struct FCLCollisionCallbackData
{
  /// Collision detector
  FCLCollisionDetector* mFclCollisionDetector;

  /// FCL collision request
  fcl::CollisionRequest mFclRequest;

  /// FCL collision result
  fcl::CollisionResult mFclResult;

  /// Collision option of DART
  const Option* mOption;

  /// Collision result of DART
  Result* mResult;

  /// Whether the collision iteration can stop
  bool done;

  /// Constructor
  FCLCollisionCallbackData(
      FCLCollisionDetector* collisionDetector,
      const Option* option = nullptr,
      Result* result = nullptr)
    : mFclCollisionDetector(collisionDetector),
      mOption(option),
      mResult(result),
      done(false)
  {
    if (mOption)
      convertOption(*mOption, mFclRequest);
  }
};

#if FCL_MAJOR_MINOR_VERSION_AT_MOST(0,3)

//==============================================================================
// Create a cube mesh for collision detection
template<class BV>
fcl::BVHModel<BV>* createCube(float _sizeX, float _sizeY, float _sizeZ)
{
  int faces[6][4] =
  {
    {0, 1, 2, 3},
    {3, 2, 6, 7},
    {7, 6, 5, 4},
    {4, 5, 1, 0},
    {5, 6, 2, 1},
    {7, 4, 0, 3}
  };
  float v[8][3];

  v[0][0] = v[1][0] = v[2][0] = v[3][0] = -_sizeX / 2;
  v[4][0] = v[5][0] = v[6][0] = v[7][0] = _sizeX / 2;
  v[0][1] = v[1][1] = v[4][1] = v[5][1] = -_sizeY / 2;
  v[2][1] = v[3][1] = v[6][1] = v[7][1] = _sizeY / 2;
  v[0][2] = v[3][2] = v[4][2] = v[7][2] = -_sizeZ / 2;
  v[1][2] = v[2][2] = v[5][2] = v[6][2] = _sizeZ / 2;

  fcl::BVHModel<BV>* model = new fcl::BVHModel<BV>;
  fcl::Vec3f p1, p2, p3;
  model->beginModel();

  for (int i = 0; i < 6; i++)
  {
    p1 = fcl::Vec3f(v[faces[i][0]][0], v[faces[i][0]][1], v[faces[i][0]][2]);
    p2 = fcl::Vec3f(v[faces[i][1]][0], v[faces[i][1]][1], v[faces[i][1]][2]);
    p3 = fcl::Vec3f(v[faces[i][2]][0], v[faces[i][2]][1], v[faces[i][2]][2]);
    model->addTriangle(p1, p2, p3);

    p1 = fcl::Vec3f(v[faces[i][0]][0], v[faces[i][0]][1], v[faces[i][0]][2]);
    p2 = fcl::Vec3f(v[faces[i][2]][0], v[faces[i][2]][1], v[faces[i][2]][2]);
    p3 = fcl::Vec3f(v[faces[i][3]][0], v[faces[i][3]][1], v[faces[i][3]][2]);
    model->addTriangle(p1, p2, p3);
  }
  model->endModel();
  return model;
}

//==============================================================================
template<class BV>
fcl::BVHModel<BV>* createEllipsoid(float _sizeX, float _sizeY, float _sizeZ)
{
  float v[59][3] =
  {
    {0, 0, 0},
    {0.135299, -0.461940, -0.135299},
    {0.000000, -0.461940, -0.191342},
    {-0.135299, -0.461940, -0.135299},
    {-0.191342, -0.461940, 0.000000},
    {-0.135299, -0.461940, 0.135299},
    {0.000000, -0.461940, 0.191342},
    {0.135299, -0.461940, 0.135299},
    {0.191342, -0.461940, 0.000000},
    {0.250000, -0.353553, -0.250000},
    {0.000000, -0.353553, -0.353553},
    {-0.250000, -0.353553, -0.250000},
    {-0.353553, -0.353553, 0.000000},
    {-0.250000, -0.353553, 0.250000},
    {0.000000, -0.353553, 0.353553},
    {0.250000, -0.353553, 0.250000},
    {0.353553, -0.353553, 0.000000},
    {0.326641, -0.191342, -0.326641},
    {0.000000, -0.191342, -0.461940},
    {-0.326641, -0.191342, -0.326641},
    {-0.461940, -0.191342, 0.000000},
    {-0.326641, -0.191342, 0.326641},
    {0.000000, -0.191342, 0.461940},
    {0.326641, -0.191342, 0.326641},
    {0.461940, -0.191342, 0.000000},
    {0.353553, 0.000000, -0.353553},
    {0.000000, 0.000000, -0.500000},
    {-0.353553, 0.000000, -0.353553},
    {-0.500000, 0.000000, 0.000000},
    {-0.353553, 0.000000, 0.353553},
    {0.000000, 0.000000, 0.500000},
    {0.353553, 0.000000, 0.353553},
    {0.500000, 0.000000, 0.000000},
    {0.326641, 0.191342, -0.326641},
    {0.000000, 0.191342, -0.461940},
    {-0.326641, 0.191342, -0.326641},
    {-0.461940, 0.191342, 0.000000},
    {-0.326641, 0.191342, 0.326641},
    {0.000000, 0.191342, 0.461940},
    {0.326641, 0.191342, 0.326641},
    {0.461940, 0.191342, 0.000000},
    {0.250000, 0.353553, -0.250000},
    {0.000000, 0.353553, -0.353553},
    {-0.250000, 0.353553, -0.250000},
    {-0.353553, 0.353553, 0.000000},
    {-0.250000, 0.353553, 0.250000},
    {0.000000, 0.353553, 0.353553},
    {0.250000, 0.353553, 0.250000},
    {0.353553, 0.353553, 0.000000},
    {0.135299, 0.461940, -0.135299},
    {0.000000, 0.461940, -0.191342},
    {-0.135299, 0.461940, -0.135299},
    {-0.191342, 0.461940, 0.000000},
    {-0.135299, 0.461940, 0.135299},
    {0.000000, 0.461940, 0.191342},
    {0.135299, 0.461940, 0.135299},
    {0.191342, 0.461940, 0.000000},
    {0.000000, -0.500000, 0.000000},
    {0.000000, 0.500000, 0.000000}
  };

  int f[112][3] =
  {
    {1, 2, 9},
    {9, 2, 10},
    {2, 3, 10},
    {10, 3, 11},
    {3, 4, 11},
    {11, 4, 12},
    {4, 5, 12},
    {12, 5, 13},
    {5, 6, 13},
    {13, 6, 14},
    {6, 7, 14},
    {14, 7, 15},
    {7, 8, 15},
    {15, 8, 16},
    {8, 1, 16},
    {16, 1, 9},
    {9, 10, 17},
    {17, 10, 18},
    {10, 11, 18},
    {18, 11, 19},
    {11, 12, 19},
    {19, 12, 20},
    {12, 13, 20},
    {20, 13, 21},
    {13, 14, 21},
    {21, 14, 22},
    {14, 15, 22},
    {22, 15, 23},
    {15, 16, 23},
    {23, 16, 24},
    {16, 9, 24},
    {24, 9, 17},
    {17, 18, 25},
    {25, 18, 26},
    {18, 19, 26},
    {26, 19, 27},
    {19, 20, 27},
    {27, 20, 28},
    {20, 21, 28},
    {28, 21, 29},
    {21, 22, 29},
    {29, 22, 30},
    {22, 23, 30},
    {30, 23, 31},
    {23, 24, 31},
    {31, 24, 32},
    {24, 17, 32},
    {32, 17, 25},
    {25, 26, 33},
    {33, 26, 34},
    {26, 27, 34},
    {34, 27, 35},
    {27, 28, 35},
    {35, 28, 36},
    {28, 29, 36},
    {36, 29, 37},
    {29, 30, 37},
    {37, 30, 38},
    {30, 31, 38},
    {38, 31, 39},
    {31, 32, 39},
    {39, 32, 40},
    {32, 25, 40},
    {40, 25, 33},
    {33, 34, 41},
    {41, 34, 42},
    {34, 35, 42},
    {42, 35, 43},
    {35, 36, 43},
    {43, 36, 44},
    {36, 37, 44},
    {44, 37, 45},
    {37, 38, 45},
    {45, 38, 46},
    {38, 39, 46},
    {46, 39, 47},
    {39, 40, 47},
    {47, 40, 48},
    {40, 33, 48},
    {48, 33, 41},
    {41, 42, 49},
    {49, 42, 50},
    {42, 43, 50},
    {50, 43, 51},
    {43, 44, 51},
    {51, 44, 52},
    {44, 45, 52},
    {52, 45, 53},
    {45, 46, 53},
    {53, 46, 54},
    {46, 47, 54},
    {54, 47, 55},
    {47, 48, 55},
    {55, 48, 56},
    {48, 41, 56},
    {56, 41, 49},
    {2, 1, 57},
    {3, 2, 57},
    {4, 3, 57},
    {5, 4, 57},
    {6, 5, 57},
    {7, 6, 57},
    {8, 7, 57},
    {1, 8, 57},
    {49, 50, 58},
    {50, 51, 58},
    {51, 52, 58},
    {52, 53, 58},
    {53, 54, 58},
    {54, 55, 58},
    {55, 56, 58},
    {56, 49, 58}
  };

  fcl::BVHModel<BV>* model = new fcl::BVHModel<BV>;
  fcl::Vec3f p1, p2, p3;
  model->beginModel();

  for (int i = 0; i < 112; i++)
  {
    p1 = fcl::Vec3f(v[f[i][0]][0] * _sizeX,
        v[f[i][0]][1] * _sizeY,
        v[f[i][0]][2] * _sizeZ);
    p2 = fcl::Vec3f(v[f[i][1]][0] * _sizeX,
        v[f[i][1]][1] * _sizeY,
        v[f[i][1]][2] * _sizeZ);
    p3 = fcl::Vec3f(v[f[i][2]][0] * _sizeX,
        v[f[i][2]][1] * _sizeY,
        v[f[i][2]][2] * _sizeZ);

    model->addTriangle(p1, p2, p3);
  }

  model->endModel();

  return model;
}

#endif

#if FCL_MAJOR_MINOR_VERSION_AT_MOST(0,4)

//==============================================================================
template<class BV>
fcl::BVHModel<BV>* createCylinder(double _baseRadius, double _topRadius,
                                  double _height, int _slices, int _stacks)
{
  const int CACHE_SIZE = 240;

  int i, j;
  float sinCache[CACHE_SIZE];
  float cosCache[CACHE_SIZE];
  float angle;
  float zBase;
  float zLow, zHigh;
  float sintemp, costemp;
  float deltaRadius;
  float radiusLow, radiusHigh;

  if (_slices >= CACHE_SIZE) _slices = CACHE_SIZE-1;

  if (_slices < 2 || _stacks < 1 || _baseRadius < 0.0 || _topRadius < 0.0 ||
      _height < 0.0)
  {
    return nullptr;
  }

  /* Center at CoM */
  zBase = -_height/2;

  /* Compute delta */
  deltaRadius = _baseRadius - _topRadius;

  /* Cache is the vertex locations cache */
  for (i = 0; i < _slices; i++)
  {
    angle = 2 * M_PI * i / _slices;
    sinCache[i] = sin(angle);
    cosCache[i] = cos(angle);
  }

  sinCache[_slices] = sinCache[0];
  cosCache[_slices] = cosCache[0];

  fcl::BVHModel<BV>* model = new fcl::BVHModel<BV>;
  fcl::Vec3f p1, p2, p3, p4;

  model->beginModel();

  /* Base of cylinder */
  sintemp = sinCache[0];
  costemp = cosCache[0];
  radiusLow = _baseRadius;
  zLow = zBase;
  p1 = fcl::Vec3f(radiusLow * sintemp, radiusLow * costemp, zLow);
  for (i = 1; i < _slices; i++)
  {
    p2 = fcl::Vec3f(radiusLow * sinCache[i], radiusLow * cosCache[i], zLow);
    p3 = fcl::Vec3f(radiusLow * sinCache[i+1], radiusLow * cosCache[i+1], zLow);
    model->addTriangle(p1, p2, p3);
  }

  /* Body of cylinder */
  for (i = 0; i < _slices; i++)
  {
    for (j = 0; j < _stacks; j++)
    {
      zLow = j * _height / _stacks + zBase;
      zHigh = (j + 1) * _height / _stacks + zBase;
      radiusLow = _baseRadius
                  - deltaRadius * (static_cast<float>(j) / _stacks);
      radiusHigh = _baseRadius
                   - deltaRadius * (static_cast<float>(j + 1) / _stacks);

      p1 = fcl::Vec3f(radiusLow * sinCache[i], radiusLow * cosCache[i],
                      zLow);
      p2 = fcl::Vec3f(radiusLow * sinCache[i+1], radiusLow * cosCache[i+1],
                      zLow);
      p3 = fcl::Vec3f(radiusHigh * sinCache[i], radiusHigh * cosCache[i],
                      zHigh);
      p4 = fcl::Vec3f(radiusHigh * sinCache[i+1], radiusHigh * cosCache[i+1],
                      zHigh);

      model->addTriangle(p1, p2, p3);
      model->addTriangle(p2, p3, p4);
    }
  }

  /* Top of cylinder */
  sintemp = sinCache[0];
  costemp = cosCache[0];
  radiusLow = _topRadius;
  zLow = zBase + _height;
  p1 = fcl::Vec3f(radiusLow * sintemp, radiusLow * costemp, zLow);
  for (i = 1; i < _slices; i++)
  {
    p2 = fcl::Vec3f(radiusLow * sinCache[i], radiusLow * cosCache[i], zLow);
    p3 = fcl::Vec3f(radiusLow * sinCache[i+1], radiusLow * cosCache[i+1], zLow);
    model->addTriangle(p1, p2, p3);
  }

  model->endModel();
  return model;
}

#endif

//==============================================================================
template<class BV>
fcl::BVHModel<BV>* createMesh(float _scaleX, float _scaleY, float _scaleZ,
                              const aiScene* _mesh)
{
  // Create FCL mesh from Assimp mesh

  assert(_mesh);
  fcl::BVHModel<BV>* model = new fcl::BVHModel<BV>;
  model->beginModel();
  for (size_t i = 0; i < _mesh->mNumMeshes; i++)
  {
    for (size_t j = 0; j < _mesh->mMeshes[i]->mNumFaces; j++)
    {
      fcl::Vec3f vertices[3];
      for (size_t k = 0; k < 3; k++)
      {
        const aiVector3D& vertex
            = _mesh->mMeshes[i]->mVertices[
              _mesh->mMeshes[i]->mFaces[j].mIndices[k]];
        vertices[k] = fcl::Vec3f(vertex.x * _scaleX,
                                 vertex.y * _scaleY,
                                 vertex.z * _scaleZ);
      }
      model->addTriangle(vertices[0], vertices[1], vertices[2]);
    }
  }
  model->endModel();
  return model;
}

//==============================================================================
template<class BV>
fcl::BVHModel<BV>* createSoftMesh(const aiMesh* _mesh)
{
  // Create FCL mesh from Assimp mesh

  assert(_mesh);
  fcl::BVHModel<BV>* model = new fcl::BVHModel<BV>;
  model->beginModel();

  for (size_t i = 0; i < _mesh->mNumFaces; i++)
  {
    fcl::Vec3f vertices[3];
    for (size_t j = 0; j < 3; j++)
    {
      const aiVector3D& vertex = _mesh->mVertices[_mesh->mFaces[i].mIndices[j]];
      vertices[j] = fcl::Vec3f(vertex.x, vertex.y, vertex.z);
    }
    model->addTriangle(vertices[0], vertices[1], vertices[2]);
  }

  model->endModel();
  return model;
}

//==============================================================================
boost::shared_ptr<fcl::CollisionGeometry> createFCLCollisionGeometry(
    const dynamics::ConstShapePtr& shape)
{
  using dynamics::Shape;
  using dynamics::BoxShape;
  using dynamics::EllipsoidShape;
  using dynamics::CylinderShape;
  using dynamics::PlaneShape;
  using dynamics::MeshShape;
  using dynamics::SoftMeshShape;

  boost::shared_ptr<fcl::CollisionGeometry> fclCollGeom;

  switch (shape->getShapeType())
  {
    case Shape::BOX:
    {
      assert(dynamic_cast<const BoxShape*>(shape.get()));

      auto box = static_cast<const BoxShape*>(shape.get());
      const Eigen::Vector3d& size = box->getSize();

#if FCL_MAJOR_MINOR_VERSION_AT_MOST(0,3)
      fclCollGeom.reset(createCube<fcl::OBBRSS>(size[0], size[1], size[2]));
#else
      fclCollGeom.reset(new fcl::Box(size[0], size[1], size[2]));
#endif

      break;
    }
    case Shape::ELLIPSOID:
    {
      assert(dynamic_cast<const EllipsoidShape*>(shape.get()));

      auto ellipsoid = static_cast<const EllipsoidShape*>(shape.get());
      const Eigen::Vector3d& size = ellipsoid->getSize();

      if (ellipsoid->isSphere())
      {
        fclCollGeom.reset(new fcl::Sphere(size[0] * 0.5));
      }
      else
      {
#if FCL_MAJOR_MINOR_VERSION_AT_MOST(0,3)
        fclCollGeom.reset(
            createEllipsoid<fcl::OBBRSS>(size[0], size[1], size[2]));
#else
        fclCollGeom.reset(
            new fcl::Ellipsoid(FCLTypes::convertVector3(size * 0.5)));
#endif
      }

      break;
    }
    case Shape::CYLINDER:
    {
      assert(dynamic_cast<const CylinderShape*>(shape.get()));

      const auto cylinder = static_cast<const CylinderShape*>(shape.get());
      const auto radius = cylinder->getRadius();
      const auto height = cylinder->getHeight();

#if FCL_MAJOR_MINOR_VERSION_AT_MOST(0,4)
      fclCollGeom.reset(createCylinder<fcl::OBBRSS>(
                          radius, radius, height, 16, 16));
#else
      fclCollGeom.reset(new fcl::Cylinder(radius, height));
#endif
      // TODO(JS): We still need to use mesh for cylinder since FCL 0.4.0
      // returns single contact point for cylinder yet.

      break;
    }
    case Shape::PLANE:
    {
#if FCL_MAJOR_MINOR_VERSION_AT_MOST(0,3)
      fclCollGeom.reset(createCube<fcl::OBBRSS>(1000.0, 0.0, 1000.0));

      dtwarn << "[FCLCollisionDetector] PlaneShape is not supported by "
             << "FCLCollisionDetector. We create a thin box mesh insted, where "
             << "the size is [1000 0 1000].\n";
#else

      assert(dynamic_cast<const PlaneShape*>(shape.get()));
      auto                  plane = static_cast<const PlaneShape*>(shape.get());
      const Eigen::Vector3d normal = plane->getNormal();
      const double          offset = plane->getOffset();
      fclCollGeom.reset(
            new fcl::Halfspace(FCLTypes::convertVector3(normal), offset));
#endif

      break;
    }
    case Shape::MESH:
    {
      assert(dynamic_cast<const MeshShape*>(shape.get()));

      auto shapeMesh = static_cast<const MeshShape*>(shape.get());
      const Eigen::Vector3d& scale = shapeMesh->getScale();
      auto aiScene = shapeMesh->getMesh();

      fclCollGeom.reset(
          createMesh<fcl::OBBRSS>(scale[0], scale[1], scale[2], aiScene));

      break;
    }
    case Shape::SOFT_MESH:
    {
      assert(dynamic_cast<const SoftMeshShape*>(shape.get()));

      auto softMeshShape = static_cast<const SoftMeshShape*>(shape.get());
      auto aiMesh = softMeshShape->getAssimpMesh();

      fclCollGeom.reset(createSoftMesh<fcl::OBBRSS>(aiMesh));

      break;
    }
    default:
    {
      dterr << "[FCLCollisionDetector] Attempting to create unsupported shape "
            << "type '" << shape->getShapeType() << "'.\n";

      return nullptr;
    }
  }

  return fclCollGeom;
}

} // anonymous namespace



//==============================================================================
std::shared_ptr<FCLCollisionDetector> FCLCollisionDetector::create()
{
  return std::shared_ptr<FCLCollisionDetector>(new FCLCollisionDetector());
}

//==============================================================================
FCLCollisionDetector::~FCLCollisionDetector()
{
  assert(mShapeMap.empty());
  assert(mFCLCollisionObjectMap.empty());
}

//==============================================================================
const std::string& FCLCollisionDetector::getTypeStatic()
{
  static const std::string& type("FCL");
  return type;
}

//==============================================================================
const std::string& FCLCollisionDetector::getType() const
{
  return FCLCollisionDetector::getTypeStatic();
}

//==============================================================================
std::shared_ptr<CollisionGroup> FCLCollisionDetector::createCollisionGroup()
{
  return std::make_shared<FCLCollisionGroup>(shared_from_this());
}

//==============================================================================
std::shared_ptr<CollisionGroup> FCLCollisionDetector::createCollisionGroup(
    const dynamics::ShapeFrame* shapeFrame)
{
  return std::make_shared<FCLCollisionGroup>(shared_from_this(), shapeFrame);
}

//==============================================================================
std::shared_ptr<CollisionGroup> FCLCollisionDetector::createCollisionGroup(
    const std::vector<const dynamics::ShapeFrame*>& shapeFrames)
{
  return std::make_shared<FCLCollisionGroup>(shared_from_this(), shapeFrames);
}

//==============================================================================
std::unique_ptr<CollisionObject>
FCLCollisionDetector::createCollisionObject(const dynamics::ShapeFrame* shapeFrame)
{
  auto fclCollGeom = claimFCLCollisionGeometry(shapeFrame->getShape());
  auto collObj = new FCLCollisionObject(this, shapeFrame, fclCollGeom);
  auto fclCollObj = collObj->getFCLCollisionObject();

  mFCLCollisionObjectMap[fclCollObj] = collObj;

  return std::unique_ptr<CollisionObject>(collObj);
}

//==============================================================================
FCLCollisionObject* FCLCollisionDetector::findCollisionObject(
    fcl::CollisionObject* fclCollObj) const
{
  auto search = mFCLCollisionObjectMap.find(fclCollObj);
  if (mFCLCollisionObjectMap.end() != search)
    return search->second;
  else
    return nullptr;
}

//==============================================================================
void FCLCollisionDetector::notifyDestroyingCollisionObject(
    CollisionObject* collObj)
{
  if (!collObj)
    return;

  reclaimFCLCollisionGeometry(collObj->getShape());

  auto casted = static_cast<FCLCollisionObject*>(collObj);
  mFCLCollisionObjectMap.erase(casted->getFCLCollisionObject());
}

//==============================================================================
boost::shared_ptr<fcl::CollisionGeometry>
FCLCollisionDetector::claimFCLCollisionGeometry(
    const dynamics::ConstShapePtr& shape)
{
  boost::shared_ptr<fcl::CollisionGeometry> fclCollGeom;

  auto search = mShapeMap.find(shape);
  if (mShapeMap.end() != search)
  {
    auto& fclCollGeomAndCount = search->second;

    fclCollGeom = search->second.first;

    auto& count = fclCollGeomAndCount.second;
    assert(0u != count);
    count++;
  }
  else
  {
    fclCollGeom = createFCLCollisionGeometry(shape);
    mShapeMap[shape] = std::make_pair(fclCollGeom, 1u);
  }

  return fclCollGeom;
}

//==============================================================================
void FCLCollisionDetector::reclaimFCLCollisionGeometry(
    const dynamics::ConstShapePtr& shape)
{
  auto search = mShapeMap.find(shape);
  assert(mShapeMap.end() != search);

  auto& fclCollGeomAndCount = search->second;
  auto& count = fclCollGeomAndCount.second;
  assert(0u != count);

  count--;

  if (0u == count)
    mShapeMap.erase(search);
}

//==============================================================================
bool FCLCollisionDetector::detect(
    CollisionGroup* group, const Option& option, Result& result)
{
  result.contacts.clear();

  if (!group)
    return false;

  if (group->getCollisionDetector()->getType()
      != FCLCollisionDetector::getTypeStatic())
  {
    return false;
  }

  group->update();

  auto castedData = static_cast<FCLCollisionGroup*>(group);
  auto broadPhaseAlg = castedData->getFCLCollisionManager();

  FCLCollisionCallbackData collData(this, &option, &result);
  broadPhaseAlg->collide(&collData, collisionCallback);

  return !result.contacts.empty();
}

//==============================================================================
bool FCLCollisionDetector::detect(
    CollisionGroup* group1, CollisionGroup* group2,
    const Option& option, Result& result)
{
  result.contacts.clear();

  if (!group1 || !group2)
    return false;

  if (group1->getCollisionDetector()->getType()
      != FCLCollisionDetector::getTypeStatic())
  {
      return false;
  }

  if (group2->getCollisionDetector()->getType()
      != FCLCollisionDetector::getTypeStatic())
  {
      return false;
  }

  group1->update();
  group2->update();

  auto casted1 = static_cast<FCLCollisionGroup*>(group1);
  auto casted2 = static_cast<FCLCollisionGroup*>(group2);

  auto broadPhaseAlg1 = casted1->getFCLCollisionManager();
  auto broadPhaseAlg2 = casted2->getFCLCollisionManager();

  FCLCollisionCallbackData collData(this, &option, &result);
  broadPhaseAlg1->collide(broadPhaseAlg2, &collData, collisionCallback);

  return !result.contacts.empty();
}



namespace {

//==============================================================================
bool collisionCallback(
    fcl::CollisionObject* o1, fcl::CollisionObject* o2, void* cdata)
{
  auto collData = static_cast<FCLCollisionCallbackData*>(cdata);

  const auto& fclRequest = collData->mFclRequest;
  auto& fclResult = collData->mFclResult;
  auto& result = *(collData->mResult);
  auto option = collData->mOption;
  auto filter = option->collisionFilter;

  if (collData->done)
    return true;

  // Filtering
  if (filter)
  {
    auto collisionDetector = collData->mFclCollisionDetector;

    auto collObj1 = collisionDetector->findCollisionObject(o1);
    auto collObj2 = collisionDetector->findCollisionObject(o2);

    if (!filter->needCollision(collObj1, collObj2))
      return collData->done;
  }

  // Clear previous results
  fclResult.clear();

  // Perform narrow-phase detection
  fcl::collide(o1, o2, fclRequest, fclResult);

  if (!fclRequest.enable_cost
      && (fclResult.isCollision())
      && ((fclResult.numContacts() >= fclRequest.num_max_contacts)))
          //|| !collData->checkAllCollisions))
    // TODO(JS): checkAllCollisions should be in FCLCollisionData
  {
    collData->done = true;
  }

  postProcess(fclResult, o1, o2, result);

  return collData->done;
}

//==============================================================================
void postProcess(const fcl::CollisionResult& fclResult,
                 fcl::CollisionObject* o1,
                 fcl::CollisionObject* o2,
                 Result& result)
{
  auto numContacts = fclResult.numContacts();

  if (0 == numContacts)
    return;

  const double ZERO = 0.000001;
  const double ZERO2 = ZERO*ZERO;

  std::vector<bool> markForDeletion(numContacts, false);

  // mark all the repeated points
  for (auto i = 0u; i < numContacts - 1; ++i)
  {
    const auto& contact1 = fclResult.getContact(i);

    for (auto j = i + 1u; j < numContacts; ++j)
    {
      const auto& contact2 = fclResult.getContact(j);

      const auto diff = contact1.pos - contact2.pos;

      if (diff.length() < 3 * ZERO2)
      {
        markForDeletion[i] = true;
        break;
      }
    }
  }

  // remove all the co-linear contact points
  for (auto i = 0u; i < numContacts; ++i)
  {
    if (markForDeletion[i])
      continue;

    const auto& contact1 = fclResult.getContact(i);

    for (auto j = i + 1u; j < numContacts; ++j)
    {
      if (markForDeletion[j])
        continue;

      const auto& contact2 = fclResult.getContact(j);

      for (auto k = j + 1u; k < numContacts; ++k)
      {
        if (markForDeletion[k])
          continue;

        const auto& contact3 = fclResult.getContact(k);

        const auto va = contact1.pos - contact2.pos;
        const auto vb = contact1.pos - contact3.pos;
        const auto v = va.cross(vb);

        if (v.length() < ZERO2)
        {
          markForDeletion[i] = true;
          break;
        }
      }
    }
  }

  for (size_t i = 0; i < numContacts; ++i)
  {
    if (!markForDeletion[i])
    {
      const auto fclContact = fclResult.getContact(i);
      result.contacts.push_back(convertContact(fclContact, o1, o2));
    }
  }
}

//==============================================================================
void convertOption(const Option& fclOption, fcl::CollisionRequest& request)
{
  request.num_max_contacts = fclOption.maxNumContacts;
  request.enable_contact   = fclOption.enableContact;
#if FCL_VERSION_AT_LEAST(0,3,0)
  request.gjk_solver_type  = fcl::GST_LIBCCD;
#endif
}

//==============================================================================
Contact convertContact(const fcl::Contact& fclContact,
                       fcl::CollisionObject* o1,
                       fcl::CollisionObject* o2)
{
  Contact contact;

  Eigen::Vector3d point = FCLTypes::convertVector3(fclContact.pos);

  contact.point = point;
  contact.normal = -FCLTypes::convertVector3(fclContact.normal);
  contact.penetrationDepth = fclContact.penetration_depth;
  contact.triID1 = fclContact.b1;
  contact.triID2 = fclContact.b2;

  auto userData1
      = static_cast<FCLCollisionObject::UserData*>(o1->getUserData());
  auto userData2
      = static_cast<FCLCollisionObject::UserData*>(o2->getUserData());
  assert(userData1);
  assert(userData2);
  contact.collisionObject1 = userData1->mCollisionObject;
  contact.collisionObject2 = userData2->mCollisionObject;

  return contact;
}

} // anonymous namespace

} // namespace collision
} // namespace dart
