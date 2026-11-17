ScriptName sslThreadController extends Quest
{build stub: in SexLab this extends sslThreadModel; the VM resolves the calls by name.
P+: GetPositions / HasSceneTag / HasStageTag. Classic: Positions / Animation.}
Actor[] property Positions auto hidden
sslBaseAnimation property Animation auto hidden
Actor[] Function GetPositions() native
bool Function HasSceneTag(String Tag)
	return false
EndFunction
bool Function HasStageTag(String Tag)
	return false
EndFunction
