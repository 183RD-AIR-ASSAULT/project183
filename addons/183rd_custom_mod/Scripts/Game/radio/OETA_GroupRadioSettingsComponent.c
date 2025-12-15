// -----------------------------------------------------------------------------
// OETA_GroupRadioSettingsComponent.c
// Optional fallback for group radio settings if no live SCR_AIGroup frequency.
// -----------------------------------------------------------------------------

class OETA_GroupRadioSettingsComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "Radio", description: "Holds group radio settings (fallback)")]
class OETA_GroupRadioSettingsComponent : ScriptComponent
{
	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox,
		desc: "Group Frequency (kHz). 0 = unset")]
	protected int m_GroupFrequencyKHz;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox,
		desc: "Group Encryption Key (optional)")]
	protected string m_GroupEncryptionKey;

	int GetGroupFrequencyKHz()
	{
		return m_GroupFrequencyKHz;
	}

	bool HasGroupEncryptionKey(out string key)
	{
		key = m_GroupEncryptionKey;
		return key != "";
	}
}
