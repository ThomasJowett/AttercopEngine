#include "vendor/doctest.h"

#include "box2d/box2d.h"

TEST_CASE("A particle's lifetime counts down with each step and it is destroyed when it runs out")
{
	const float timeStep = 1.0f / 60.0f;

	b2World world(b2Vec2(0.0f, 0.0f));
	b2ParticleSystemDef systemDef;
	b2ParticleSystem* system = world.CreateParticleSystem(&systemDef);

	b2ParticleDef particleDef;
	particleDef.lifetime = 0.5f;
	system->CreateParticle(particleDef);
	REQUIRE(system->GetParticleCount() == 1);

	for (int i = 0; i < 15; i++)
		world.Step(timeStep, 8, 3, 1);
	REQUIRE(system->GetParticleCount() == 1);
	CHECK(system->GetParticleLifetime(0) == doctest::Approx(0.25f).epsilon(0.05));

	for (int i = 0; i < 20; i++)
		world.Step(timeStep, 8, 3, 1);
	CHECK(system->GetParticleCount() == 0);
}
