// Holds the list of faction keys that can see shared markers.
// Put this on your GameMode entity (recommended).

class OETA_MarkerShareConfigComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "OETA|Markers", description: "Config: which factions can see shared markers.")]
class OETA_MarkerShareConfigComponent : ScriptComponent
{
	// Comma-separated faction keys, e.g.:
	// "BLUFOR,INDFOR" or "183RD_BLUFOR"
	[Attribute("BLUFOR", UIWidgets.EditBox, "Factions that share markers (keys, comma-separated)",
		"Example: BLUFOR,INDFOR. These factions will all see player+GM placed markers.")]
	protected string m_SharedFactionKeys;

	string GetSharedFactionKeys()
	{
		return m_SharedFactionKeys;
	}
}
