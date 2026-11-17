Scriptname Afterglow_PlayerAlias extends ReferenceAlias
{Afterglow 2.0: re-registers the SexLab events after every load.}

Event OnPlayerLoadGame()
	(GetOwningQuest() as Afterglow_SexLab).Maintenance()
EndEvent
