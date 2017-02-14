// Fill out your copyright notice in the Description page of Project Settings.

#include "unreal.h"
#include "TransformSender.h"
#include "Conversions.h"
#include "EntityId.h"
#include "WorkerConnection.h"
#include "improbable/corelibrary/transforms/transform_state.h"
#include "unrealGameMode.h"

using namespace improbable::unreal::core;

// Sets default values for this component's properties
UTransformSender::UTransformSender()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You
    // can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;
}

// Called when the game starts
void UTransformSender::BeginPlay()
{
    Super::BeginPlay();
    EntityId = AunrealGameMode::GetSpawner()->GetEntityId(GetOwner());
}

// Called every frame
void UTransformSender::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (EntityId == -1)
    {
        EntityId = AunrealGameMode::GetSpawner()->GetEntityId(GetOwner());

        if (EntityId != -1)
        {
            const auto* const entity = GetEntity();

            if (entity != nullptr)
            {
                worker::Option<improbable::corelibrary::transforms::TransformStateData> transform =
                    entity->Get<improbable::corelibrary::transforms::TransformState>();

                if (!transform.empty() && HasAuthority())
                {
                    FVector location = ToUnrealVector(transform->local_position());
                    FQuat rotation = ToUnrealQuaternion(transform->local_rotation().quaternion());

                    auto* const owner = GetOwner();
                    owner->SetActorLocation(location);
                    owner->SetActorRotation(rotation);
                }
            }
        }

        return;
    }

    if (!HasAuthority())
	{
        return;
    }

    FVector location = GetOwner()->GetActorLocation();
    improbable::corelibrary::math::FixedPointVector3 locationUpdate =
        ToFixedPoint(UnrealToSpatialPosition(location));

    FQuat rotation = GetOwner()->GetActorRotation().Quaternion();
    uint32_t rotationUpdate = ToQuaternion32(UnrealToSpatialQuaternion(rotation));

    auto entity = GetEntity();
    if (entity != nullptr)
    {
        auto previous = entity->Get<improbable::corelibrary::transforms::TransformState>();
        if (previous.empty() || previous->local_position() != locationUpdate ||
            previous->local_rotation() != rotationUpdate)
        {
            improbable::corelibrary::transforms::TransformState::Update update;
            update.set_local_position(locationUpdate);
            update.set_local_rotation(rotationUpdate);
			
            entity->Update<improbable::corelibrary::transforms::TransformState>(update);
            FWorkerConnection::GetConnection()
                .SendComponentUpdate<improbable::corelibrary::transforms::TransformState>(EntityId,
                                                                                          update);
        }
    }
}

bool UTransformSender::HasAuthority() const
{
    if (GetEntity() == nullptr)
    {
        return false;
    }

    return GetEntity()->HasAuthority<improbable::corelibrary::transforms::TransformState>();
}

worker::Entity* UTransformSender::GetEntity() const
{
    auto it = FWorkerConnection::GetView().Entities.find(EntityId);
    if (it == FWorkerConnection::GetView().Entities.end())
    {
        UE_LOG(LogTemp, Warning,
               TEXT("UTransformSender: returned nullptr trying to get entity %s on Actor %s"),
               *ToString(EntityId), *GetOwner()->GetName())
        return nullptr;
    }

    return &(it->second);
}
