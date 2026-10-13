// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "TEAMBOXCharacter.generated.h"

class UInputComponent;
class USkeletalMeshComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ATEAMBOXEnergyCore;
class ATEAMBOXSwitchLever;
class USoundBase;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

UCLASS(config=Game)
class ATEAMBOXCharacter : public ACharacter
{
	GENERATED_BODY()

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* LookAction;

	/** Summon Energy Core Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* SummonEnergyCoreAction;

	/** Pick Up / Drop Energy Core Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* PickUpDropAction;

	/** Reset Energy Core Rotation Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* ResetEnergyCoreRotationAction;

	/** Rotate Energy Core Q Key Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* RotateEnergyCoreQAction;

	/** Rotate Energy Core E Key Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* RotateEnergyCoreEAction;

	/** Adjust Energy Core Distance Input Action (Mouse Wheel) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* AdjustEnergyCoreDistanceAction;

	/** Interact Input Action (e.g. G key for lever switch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	class UInputAction* InteractAction;

public:
	ATEAMBOXCharacter();

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;

	/** Energy Core class to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy Core")
	TSubclassOf<class ATEAMBOXEnergyCore> EnergyCoreClass;

	/** Distance in front of camera to spawn energy core (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy Core", meta = (ClampMin = "100.0", ClampMax = "10000.0"))
	float SummonDistance;

	/** Maximum number of energy cores that can exist at once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy Core", meta = (ClampMin = "1", ClampMax = "10"))
	int32 MaxEnergyCoreCount;

	/** Maximum distance for picking up energy cores (in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy Core", meta = (ClampMin = "100.0", ClampMax = "10000.0"))
	float PickUpRange;

	/** Scroll speed for adjusting energy core distance (distance change per scroll, in cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Energy Core", meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	float EnergyCoreDistanceScrollSpeed;

	/** 召唤能量核心（F键）成功时播放的音效（在角色 BP/Details 中配置） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Energy Core|Audio")
	USoundBase* SummonEnergyCoreSFX;

protected:
	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called to summon an energy core */
	void SummonEnergyCore();

	/** Called to pick up or drop an energy core */
	void PickUpOrDropEnergyCore();

	/** Called to reset the rotation of held energy cores */
	void ResetEnergyCoreRotation();

	/** Called to rotate energy core on Z axis (Q key) */
	void RotateEnergyCoreZ();

	/** Called to rotate energy core on Y axis (E key) */
	void RotateEnergyCoreY();

	/** Called to adjust energy core distance (mouse wheel) */
	void AdjustEnergyCoreDistance(const FInputActionValue& Value);

	/** Called when InteractAction is pressed (e.g. toggle lever in range) */
	void TryInteract();

protected:
	// AActor interface
	virtual void BeginPlay() override;
	// End of AActor interface

	// APawn interface
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	/** 拉杆开关在重叠时调用，用于按 G 交互 */
	void RegisterOverlappingLever(ATEAMBOXSwitchLever* Lever);
	void UnregisterOverlappingLever(ATEAMBOXSwitchLever* Lever);

private:
	/** List of spawned energy cores (FIFO queue) */
	UPROPERTY()
	TArray<ATEAMBOXEnergyCore*> SpawnedEnergyCores;

	/** Currently held energy core (nullptr if not holding any) */
	UPROPERTY()
	ATEAMBOXEnergyCore* HeldEnergyCore;

	/** Last operated energy core (last summoned or last picked up) */
	UPROPERTY()
	ATEAMBOXEnergyCore* LastOperatedEnergyCore;

	/** 当前与玩家重叠的拉杆开关（按 G 时切换第一个） */
	UPROPERTY()
	TArray<ATEAMBOXSwitchLever*> OverlappingLevers;
};

