#include "DeadHospitalSafePuzzle.h"

ADeadHospitalSafePuzzle::ADeadHospitalSafePuzzle()
{
	PuzzleId = TEXT("PZ02");
	InteractionText = FText::FromString(TEXT("Press E to inspect safe"));
}

bool ADeadHospitalSafePuzzle::SubmitCode(
	AActor* Interactor,
	const FString& EnteredCode)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}

	const FString TrimmedCode = EnteredCode.TrimStartAndEnd();

	if (TrimmedCode != CorrectCode)
	{
		OnCodeRejected(TrimmedCode);
		return false;
	}

	if (!TryCompletePuzzle(Interactor))
	{
		return false;
	}

	OnCodeAccepted();

	return true;
}