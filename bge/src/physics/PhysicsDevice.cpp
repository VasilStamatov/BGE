#include "physics/PhysicsDevice.h"

#include "logging/Log.h"

#include <nudge/nudge.h>

#include <immintrin.h>

namespace bge
{

struct RigidBody
{
  uint32 m_ColliderId{0};
  uint32 m_BodyId{0};
};

static const uint32 s_MaxBodyCount = 2048;
static const uint32 s_MaxBoxCount = 2048;
static const uint32 s_MaxSphereCount = 2048;
static const uint32 s_Steps = 2;
static const uint32 s_Iterations = 20;
static const float s_TimeStep = 1.0f / (60.0f * (float)s_Steps);
static const float s_Damping = 1.0f - s_TimeStep * 0.25f;

static nudge::Arena s_Arena;
static nudge::BodyData s_Bodies;
static nudge::ColliderData s_Colliders;
static nudge::ContactData s_ContactData;
static nudge::ContactCache s_ContactCache;
static nudge::ActiveBodies s_ActiveBodies;

static std::unordered_map<uint32, RigidBody> s_EntityToRigidBody;
static std::vector<Entity> s_EntitiesWithBodies;

static const nudge::Transform s_IdentityTransform = {
    {}, 0, {0.0f, 0.0f, 0.0f, 1.0f}};

namespace PhysicsDevice
{

void Initialize()
{
  // Print information about instruction set.
#ifdef __AVX2__
  BGE_CORE_INFO("Using 8-wide AVX");
#else
  BGE_CORE_INFO("Using 4-wide SSE");
#if defined(__SSE4_1__) || defined(__AVX__)
  BGE_CORE_INFO("BLENDVPS: Enabled");
#else
  BGE_CORE_INFO("BLENDVPS: Disabled");
#endif
#endif

#ifdef __FMA__
  BGE_CORE_INFO("FMA: Enabled");
#else
  BGE_CORE_INFO("FMA: Disabled");
#endif

  // Allocate memory for simulation s_Arena.
  s_Arena.size = 64 * 1024 * 1024; // 64 MegaBytes
  s_Arena.data = _mm_malloc(s_Arena.size, 4096);

  // Allocate memory for s_Bodies, s_Colliders, and contacts.
  s_ActiveBodies.capacity = s_MaxBoxCount;
  s_ActiveBodies.indices =
      static_cast<uint16_t*>(_mm_malloc(sizeof(uint16_t) * s_MaxBodyCount, 64));

  s_Bodies.idle_counters =
      static_cast<uint8_t*>(_mm_malloc(sizeof(uint8_t) * s_MaxBodyCount, 64));
  s_Bodies.transforms = static_cast<nudge::Transform*>(
      _mm_malloc(sizeof(nudge::Transform) * s_MaxBodyCount, 64));
  s_Bodies.momentum = static_cast<nudge::BodyMomentum*>(
      _mm_malloc(sizeof(nudge::BodyMomentum) * s_MaxBodyCount, 64));
  s_Bodies.properties = static_cast<nudge::BodyProperties*>(
      _mm_malloc(sizeof(nudge::BodyProperties) * s_MaxBodyCount, 64));

  s_Colliders.boxes.data = static_cast<nudge::BoxCollider*>(
      _mm_malloc(sizeof(nudge::BoxCollider) * s_MaxBoxCount, 64));
  s_Colliders.boxes.tags =
      static_cast<uint16_t*>(_mm_malloc(sizeof(uint16_t) * s_MaxBoxCount, 64));
  s_Colliders.boxes.transforms = static_cast<nudge::Transform*>(
      _mm_malloc(sizeof(nudge::Transform) * s_MaxBoxCount, 64));

  s_Colliders.spheres.data = static_cast<nudge::SphereCollider*>(
      _mm_malloc(sizeof(nudge::SphereCollider) * s_MaxSphereCount, 64));
  s_Colliders.spheres.tags = static_cast<uint16_t*>(
      _mm_malloc(sizeof(uint16_t) * s_MaxSphereCount, 64));
  s_Colliders.spheres.transforms = static_cast<nudge::Transform*>(
      _mm_malloc(sizeof(nudge::Transform) * s_MaxSphereCount, 64));

  s_ContactData.capacity = s_MaxBodyCount * 64;
  s_ContactData.bodies = static_cast<nudge::BodyPair*>(
      _mm_malloc(sizeof(nudge::BodyPair) * s_ContactData.capacity, 64));
  s_ContactData.data = static_cast<nudge::Contact*>(
      _mm_malloc(sizeof(nudge::Contact) * s_ContactData.capacity, 64));
  s_ContactData.tags = static_cast<uint64_t*>(
      _mm_malloc(sizeof(uint64_t) * s_ContactData.capacity, 64));
  s_ContactData.sleeping_pairs = static_cast<uint32_t*>(
      _mm_malloc(sizeof(uint32_t) * s_ContactData.capacity, 64));

  s_ContactCache.capacity = s_MaxBodyCount * 64;
  s_ContactCache.data = static_cast<nudge::CachedContactImpulse*>(_mm_malloc(
      sizeof(nudge::CachedContactImpulse) * s_ContactCache.capacity, 64));
  s_ContactCache.tags = static_cast<uint64_t*>(
      _mm_malloc(sizeof(uint64_t) * s_ContactCache.capacity, 64));

  // The first body is the static world.
  s_Bodies.count = 1;
  s_Bodies.idle_counters[0] = 0;
  s_Bodies.transforms[0] = s_IdentityTransform;
  memset(s_Bodies.momentum, 0, sizeof(s_Bodies.momentum[0]));
  memset(s_Bodies.properties, 0, sizeof(s_Bodies.properties[0]));
}

CollidedBodies Simulate()
{
  for (uint32 n = 0; n < s_Steps; ++n)
  {
    // Setup a temporary memory s_Arena. The same temporary memory is reused
    // each iteration.
    nudge::Arena temporary = s_Arena;

    // Find contacts.
    // NOTE: Custom constraints should be added as body connections.
    nudge::BodyConnections connections = {};

    nudge::collide(&s_ActiveBodies, &s_ContactData, s_Bodies, s_Colliders,
                   connections, temporary);

    CollidedBodies collidedBodies;
    collidedBodies.m_Count = s_ContactData.count;
    memcpy(collidedBodies.m_Bodies, s_ContactData.bodies,
           s_ContactData.count * sizeof(s_ContactData.bodies[0]));

    // NOTE: Custom contacts can be added here, e.g., against the static
    // environment.

    // Apply gravity and damping.
    for (uint32 i = 0; i < s_ActiveBodies.count; ++i)
    {
      uint32 index = s_ActiveBodies.indices[i];

      s_Bodies.momentum[index].velocity[1] -= 9.82f * s_TimeStep;

      s_Bodies.momentum[index].velocity[0] *= s_Damping;
      s_Bodies.momentum[index].velocity[1] *= s_Damping;
      s_Bodies.momentum[index].velocity[2] *= s_Damping;

      s_Bodies.momentum[index].angular_velocity[0] *= s_Damping;
      s_Bodies.momentum[index].angular_velocity[1] *= s_Damping;
      s_Bodies.momentum[index].angular_velocity[2] *= s_Damping;
    }

    // Read previous impulses from contact cache.
    nudge::ContactImpulseData* contactImpulses =
        nudge::read_cached_impulses(s_ContactCache, s_ContactData, &temporary);

    // Setup contact constraints and apply the initial impulses.
    nudge::ContactConstraintData* contactConstraints =
        nudge::setup_contact_constraints(s_ActiveBodies, s_ContactData,
                                         s_Bodies, contactImpulses, &temporary);

    // Apply contact impulses. Increasing the number of iterations will improve
    // stability.
    for (uint32 i = 0; i < s_Iterations; ++i)
    {
      nudge::apply_impulses(contactConstraints, s_Bodies);
      // NOTE: Custom constraint impulses should be applied here.
    }

    // Update contact impulses.
    nudge::update_cached_impulses(contactConstraints, contactImpulses);

    // Write the updated contact impulses to the cache.
    nudge::write_cached_impulses(&s_ContactCache, s_ContactData,
                                 contactImpulses);

    // Move active s_Bodies.
    nudge::advance(s_ActiveBodies, s_Bodies, s_TimeStep);

    // return collided bodies
    return collidedBodies;
  }
}

// uint32 MakeBoxCollider(float position[3], float rotation[4], float size[3])
// {
//   BGE_CORE_ASSERT(s_Colliders.boxes.count < s_MaxBoxCount,
//                   "Max count colliders reached");

//   uint32 collider = s_Colliders.boxes.count++;

//   memcpy(s_Colliders.boxes.transforms[collider].position, position,
//          sizeof(position[0]) * 3);

//   memcpy(s_Colliders.boxes.transforms[collider].rotation, rotation,
//          sizeof(rotation[0]) * 4);

//   memcpy(s_Colliders.boxes.data[collider].size, size, sizeof(size[0]) * 3);

//   s_Colliders.boxes.tags[collider] = collider;

//   return collider;
// }

// uint32 MakeSphereCollider(float position[3], float radius)
// {
//   BGE_CORE_ASSERT(s_Colliders.spheres.count <= s_MaxSphereCount,
//                   "Max count colliders reached");

//   uint32 collider = s_Colliders.spheres.count++;

//   s_Colliders.spheres.transforms[collider] = s_IdentityTransform;

//   memcpy(s_Colliders.spheres.transforms[collider].position, position,
//          sizeof(position[0]) * 3);

//   s_Colliders.spheres.data[collider].radius = radius;

//   s_Colliders.spheres.tags[collider] = collider + s_MaxBoxCount;

//   return collider;
// }

// uint32 MakeBoxBody(float mass, float cx, float cy, float cz)
// {
//   BGE_CORE_ASSERT(s_Bodies.count < s_MaxBodyCount, "Max count bodies
//   reached"); uint32 body = s_Bodies.count++;

//   float k = mass * (1.0f / 3.0f);

//   float kcx2 = k * cx * cx;
//   float kcy2 = k * cy * cy;
//   float kcz2 = k * cz * cz;

//   nudge::BodyProperties properties = {};
//   properties.mass_inverse = 1.0f / mass;
//   properties.inertia_inverse[0] = 1.0f / (kcy2 + kcz2);
//   properties.inertia_inverse[1] = 1.0f / (kcx2 + kcz2);
//   properties.inertia_inverse[2] = 1.0f / (kcx2 + kcy2);

//   memset(&s_Bodies.momentum[body], 0, sizeof(s_Bodies.momentum[body]));
//   s_Bodies.idle_counters[body] = 0;
//   s_Bodies.properties[body] = properties;
//   s_Bodies.transforms[body] = s_IdentityTransform;

//   return body;
// }

// uint32 MakeSphereBody(float mass, float radius)
// {
//   BGE_CORE_ASSERT(s_Bodies.count < s_MaxBodyCount, "Max count bodies
//   reached");

//   uint32 body = s_Bodies.count++;

//   float k = 2.5f / (mass * radius * radius);

//   nudge::BodyProperties properties = {};
//   properties.mass_inverse = 1.0f / mass;
//   properties.inertia_inverse[0] = k;
//   properties.inertia_inverse[1] = k;
//   properties.inertia_inverse[2] = k;

//   memset(&s_Bodies.momentum[body], 0, sizeof(s_Bodies.momentum[body]));
//   s_Bodies.idle_counters[body] = 0;
//   s_Bodies.properties[body] = properties;
//   s_Bodies.transforms[body] = s_IdentityTransform;

//   return body;
// }

void CreateBox(Entity entity, float mass, float cx, float cy, float cz)
{
  BGE_CORE_ASSERT(s_Bodies.count < s_MaxBodyCount, "Max count bodies reached");
  BGE_CORE_ASSERT(s_Colliders.boxes.count < s_MaxBoxCount,
                  "Max count colliders reached");
  BGE_CORE_ASSERT(s_EntityToRigidBody.count(entity.GetId()) == 0,
                  "This entity already has a rigid body!");

  uint32 body = s_Bodies.count++;
  uint32 collider = s_Colliders.boxes.count++;
  s_EntitiesWithBodies.push_back(entity);

  float k = mass * (1.0f / 3.0f);

  float kcx2 = k * cx * cx;
  float kcy2 = k * cy * cy;
  float kcz2 = k * cz * cz;

  nudge::BodyProperties properties = {};
  properties.mass_inverse = 1.0f / mass;
  properties.inertia_inverse[0] = 1.0f / (kcy2 + kcz2);
  properties.inertia_inverse[1] = 1.0f / (kcx2 + kcz2);
  properties.inertia_inverse[2] = 1.0f / (kcx2 + kcy2);

  memset(&s_Bodies.momentum[body], 0, sizeof(s_Bodies.momentum[body]));
  s_Bodies.idle_counters[body] = 0;
  s_Bodies.properties[body] = properties;
  s_Bodies.transforms[body] = s_IdentityTransform;

  s_Colliders.boxes.transforms[collider] = s_IdentityTransform;
  s_Colliders.boxes.transforms[collider].body = body;

  s_Colliders.boxes.data[collider].size[0] = cx;
  s_Colliders.boxes.data[collider].size[1] = cy;
  s_Colliders.boxes.data[collider].size[2] = cz;
  s_Colliders.boxes.tags[collider] = collider;

  RigidBody rigidBody{collider, body};
  s_EntityToRigidBody[entity.GetId()] = rigidBody;
}

void CreateSphere(Entity entity, float mass, float radius)
{
  BGE_CORE_ASSERT(s_Bodies.count < s_MaxBodyCount, "Max count bodies reached");
  BGE_CORE_ASSERT(s_Colliders.spheres.count < s_MaxSphereCount,
                  "Max count colliders reached");
  BGE_CORE_ASSERT(s_EntityToRigidBody.count(entity.GetId()) == 0,
                  "This entity already has a rigid body!");

  uint32 body = s_Bodies.count++;
  uint32 collider = s_Colliders.spheres.count++;
  s_EntitiesWithBodies.push_back(entity);

  float k = 2.5f / (mass * radius * radius);

  nudge::BodyProperties properties = {};
  properties.mass_inverse = 1.0f / mass;
  properties.inertia_inverse[0] = k;
  properties.inertia_inverse[1] = k;
  properties.inertia_inverse[2] = k;

  memset(&s_Bodies.momentum[body], 0, sizeof(s_Bodies.momentum[body]));
  s_Bodies.idle_counters[body] = 0;
  s_Bodies.properties[body] = properties;
  s_Bodies.transforms[body] = s_IdentityTransform;

  s_Colliders.spheres.transforms[collider] = s_IdentityTransform;
  s_Colliders.spheres.transforms[collider].body = body;

  s_Colliders.spheres.data[collider].radius = radius;
  s_Colliders.spheres.tags[collider] = collider + s_MaxBoxCount;

  RigidBody rigidBody{collider, body};
  s_EntityToRigidBody[entity.GetId()] = rigidBody;
}

void DestroyBox(Entity entity)
{
  RigidBody rb = s_EntityToRigidBody[entity.GetId()];

  uint32 lastBodyId = --s_Bodies.count;
  uint32 lastColliderId = --s_Colliders.boxes.count;
  Entity lastEntity = s_EntitiesWithBodies[lastBodyId];

  s_Bodies.idle_counters[rb.m_BodyId] = s_Bodies.idle_counters[lastBodyId];
  s_Bodies.momentum[rb.m_BodyId] = s_Bodies.momentum[lastBodyId];
  s_Bodies.properties[rb.m_BodyId] = s_Bodies.properties[lastBodyId];
  s_Bodies.transforms[rb.m_BodyId] = s_Bodies.transforms[lastBodyId];

  s_EntitiesWithBodies[rb.m_BodyId] = s_EntitiesWithBodies[lastBodyId];
  s_EntitiesWithBodies.pop_back();

  s_Colliders.boxes.data[rb.m_ColliderId] =
      s_Colliders.boxes.data[lastColliderId];

  s_Colliders.boxes.tags[rb.m_ColliderId] =
      s_Colliders.boxes.tags[lastColliderId];

  s_Colliders.boxes.transforms[rb.m_ColliderId] =
      s_Colliders.boxes.transforms[lastColliderId];

  s_EntityToRigidBody[lastEntity.GetId()] = rb;
  s_EntityToRigidBody.erase(entity.GetId());
}

void DestroySphere(Entity entity)
{
  RigidBody rb = s_EntityToRigidBody[entity.GetId()];

  uint32 lastBodyId = --s_Bodies.count;
  uint32 lastColliderId = --s_Colliders.spheres.count;
  Entity lastEntity = s_EntitiesWithBodies[lastBodyId];

  s_Bodies.idle_counters[rb.m_BodyId] = s_Bodies.idle_counters[lastBodyId];
  s_Bodies.momentum[rb.m_BodyId] = s_Bodies.momentum[lastBodyId];
  s_Bodies.properties[rb.m_BodyId] = s_Bodies.properties[lastBodyId];
  s_Bodies.transforms[rb.m_BodyId] = s_Bodies.transforms[lastBodyId];

  s_EntitiesWithBodies[rb.m_BodyId] = s_EntitiesWithBodies[lastBodyId];
  s_EntitiesWithBodies.pop_back();

  s_Colliders.spheres.data[rb.m_ColliderId] =
      s_Colliders.spheres.data[lastColliderId];

  s_Colliders.spheres.tags[rb.m_ColliderId] =
      s_Colliders.spheres.tags[lastColliderId];

  s_Colliders.spheres.transforms[rb.m_ColliderId] =
      s_Colliders.spheres.transforms[lastColliderId];

  s_EntityToRigidBody[lastEntity.GetId()] = rb;
  s_EntityToRigidBody.erase(entity.GetId());
}

// void SetBodyPosition(uint32 bodyId, float position[3])
// {
//   BGE_CORE_ASSERT(bodyId < s_Bodies.count, "Invalid body Id");

//   memcpy(s_Bodies.transforms[bodyId].position, position,
//          sizeof(position[0]) * 3);
// }

// void SetBoxColliderPosition(uint32 colliderId, float position[3])
// {
//   BGE_CORE_ASSERT(colliderId < s_Colliders.boxes.count,
//                   "Invalid box collider Id");

//   memcpy(s_Colliders.boxes.transforms[colliderId].position, position,
//          sizeof(position[0]) * 3);
// }

// void SetBoxColliderSize(uint32 colliderId, float size[3])
// {
//   BGE_CORE_ASSERT(colliderId < s_Colliders.boxes.count,
//                   "Invalid box collider Id");

//   memcpy(s_Colliders.boxes.data[colliderId].size, size, sizeof(size[0]) * 3);
// }

// void SetBoxColliderBody(uint32 colliderId, uint32 bodyId)
// {
//   BGE_CORE_ASSERT(colliderId < s_Colliders.boxes.count,
//                   "Invalid box collider Id");

//   s_Colliders.boxes.transforms[colliderId].body = bodyId;
// }

// void SetSphereColliderPosition(uint32 colliderId, float position[3])
// {
//   BGE_CORE_ASSERT(colliderId < s_Colliders.spheres.count,
//                   "Invalid box collider Id");

//   memcpy(s_Colliders.spheres.transforms[colliderId].position, position,
//          sizeof(position[0]) * 3);
// }

// void SetSphereColliderRadius(uint32 colliderId, float radius)
// {
//   BGE_CORE_ASSERT(colliderId < s_Colliders.spheres.count,
//                   "Invalid box collider Id");

//   s_Colliders.spheres.data[colliderId].radius = radius;
// }

// void SetSphereColliderBody(uint32 colliderId, uint32 bodyId)
// {
//   BGE_CORE_ASSERT(colliderId < s_Colliders.spheres.count,
//                   "Invalid box collider Id");

//   s_Colliders.spheres.transforms[colliderId].body = bodyId;
// }

void GetBodyTransform(Entity entity, Transform& output)
{
  BGE_CORE_ASSERT(s_EntityToRigidBody.count(entity.GetId()),
                  "Entity not registered with a body.");
  RigidBody rb = s_EntityToRigidBody[entity.GetId()];

  output.SetTranslation(Vec3f(s_Bodies.transforms[rb.m_BodyId].position));
  output.SetRotation(Quatf(s_Bodies.transforms[rb.m_BodyId].rotation));
}

Vec3f GetBoxColliderScale(Entity entity)
{
  BGE_CORE_ASSERT(s_EntityToRigidBody.count(entity.GetId()),
                  "Entity not registered with a body.");

  RigidBody rb = s_EntityToRigidBody[entity.GetId()];

  return Vec3f(s_Colliders.boxes.data[rb.m_ColliderId].size);
}

float GetSphereColliderRadius(Entity entity)
{
  BGE_CORE_ASSERT(s_EntityToRigidBody.count(entity.GetId()),
                  "Entity not registered with a body.");

  RigidBody rb = s_EntityToRigidBody[entity.GetId()];

  return s_Colliders.spheres.data[rb.m_ColliderId].radius;
}

} // namespace PhysicsDevice
} // namespace bge
