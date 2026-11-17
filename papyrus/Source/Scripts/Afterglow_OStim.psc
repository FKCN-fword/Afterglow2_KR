Scriptname Afterglow_OStim extends ReferenceAlias
{Afterglow 2.0 bridge for OStim Standalone. Lives on its own alias: without OStim installed only this
script fails to link, the SexLab bridge keeps working. On "ostim_actor_orgasm" it looks at the scene's
actions between the actor who came and each partner, folds them into Afterglow's zone bits and makes
one native call per partner.}

Event OnInit()
	Register()
EndEvent

Event OnPlayerLoadGame()
	Register()
EndEvent

Function Register()
	If Game.IsPluginInstalled("OStim.esp")
		RegisterForModEvent("ostim_actor_orgasm", "OnOStimOrgasm")
	EndIf
EndFunction

; sent by OStim as SendModEvent: strArg = scene id, numArg = thread id, sender = the actor who came
Event OnOStimOrgasm(String asEventName, String asSceneID, Float afThreadID, Form akSender)
	Actor src = akSender as Actor
	If !src
		Return
	EndIf
	Int tid = afThreadID as Int
	Actor[] actors = OThread.GetActors(tid)
	If !actors
		Return
	EndIf
	String sceneID = OThread.GetScene(tid)
	Int srcPos = actors.Find(src)
	Int srcSex = SexOf(src)
	Int i = 0
	While i < actors.Length
		If i != srcPos && actors[i]
			Actor[] one = new Actor[1]
			one[0] = actors[i]
			Int[] sexes = new Int[1]
			sexes[0] = SexOf(actors[i])
			Afterglow.OnOrgasm(src, srcSex, one, sexes, MaskFor(sceneID, srcPos, i))
		EndIf
		i += 1
	EndWhile
EndEvent

; 0 male, 1 female, 2 futa, 3 creature male, 4 creature female (Afterglow / SexLab P+ codes)
Int Function SexOf(Actor akActor)
	Bool creature = !akActor.HasKeywordString("ActorTypeNPC")
	Bool schlong = OActorUtil.HasSchlong(akActor)
	If akActor.GetLeveledActorBase().GetSex() == 1
		If creature
			Return 4
		ElseIf schlong
			Return 2
		EndIf
		Return 1
	EndIf
	If creature
		Return 3
	EndIf
	Return 0
EndFunction

; bits: 1 vaginal, 2 anal, 4 oral, 8 breast, 16 hands, 32 feet (Afterglow.dll TagBit)
Int Function MaskFor(String asScene, Int aiA, Int aiB)
	Int mask = 0
	If Pair(asScene, aiA, aiB, "vaginalsex") || Pair(asScene, aiA, aiB, "vaginaltailsex") || Pair(asScene, aiA, aiB, "cumonvulva")
		mask += 1
	EndIf
	If Pair(asScene, aiA, aiB, "analsex") || Pair(asScene, aiA, aiB, "analtailsex") || Pair(asScene, aiA, aiB, "cumonbutt")
		mask += 2
	EndIf
	If Pair(asScene, aiA, aiB, "blowjob") || Pair(asScene, aiA, aiB, "deepthroating") || Pair(asScene, aiA, aiB, "facial") || Pair(asScene, aiA, aiB, "penilelicking") || Pair(asScene, aiA, aiB, "rubbingpenisagainstface")
		mask += 4
	EndIf
	If Pair(asScene, aiA, aiB, "boobjob") || Pair(asScene, aiA, aiB, "cumonchest")
		mask += 8
	EndIf
	If Pair(asScene, aiA, aiB, "handjob")
		mask += 16
	EndIf
	If Pair(asScene, aiA, aiB, "footjob")
		mask += 32
	EndIf
	Return mask
EndFunction

; true when the scene has an action of this type between positions aiA and aiB (either role)
Bool Function Pair(String asScene, Int aiA, Int aiB, String asType)
	Int[] idx = OMetadata.FindActions(asScene, asType)
	If !idx
		Return False
	EndIf
	Int k = 0
	While k < idx.Length
		Int doer = OMetadata.GetActionActor(asScene, idx[k])
		Int receiver = OMetadata.GetActionTarget(asScene, idx[k])
		If (doer == aiA && receiver == aiB) || (doer == aiB && receiver == aiA)
			Return True
		EndIf
		k += 1
	EndWhile
	Return False
EndFunction
