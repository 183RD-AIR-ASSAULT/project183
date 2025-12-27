// -----------------------------------------------------------------------------
// OETA_RadioChannelNamesComponent.c
// Holds human-readable UI text for radio channels on a player entity.
// -----------------------------------------------------------------------------

class OETA_RadioChannelNamesComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "Radio", description: "Holds human-readable text per radio channel")]
class OETA_RadioChannelNamesComponent : ScriptComponent
{
	protected ref map<int, string> m_ChannelText;

	protected void EnsureMap()
	{
		if (!m_ChannelText)
			m_ChannelText = new map<int, string>();
	}

	// Set text for a channel (empty string = clear)
	void SetChannelText(int index, string text)
	{
		EnsureMap();

		if (text == "")
		{
			m_ChannelText.Remove(index);
			return;
		}

		m_ChannelText.Set(index, text);
	}

	// Get text for a channel ("" if none)
	string GetChannelText(int index)
	{
		if (!m_ChannelText)
			return "";

		string outText;
		if (m_ChannelText.Find(index, outText))
			return outText;

		return "";
	}

	// Remove text from a range of channels
	void ClearTextRange(int startIndex, int count)
	{
		if (!m_ChannelText)
			return;

		for (int i = 0; i < count; i++)
			m_ChannelText.Remove(startIndex + i);
	}

	// Utility for building a UI label, e.g. "3: 45250 kHz (TankCmd)"
	string FormatChannelText(BaseRadioComponent radio, int channelIndex)
	{
		if (!radio)
			return "";

		int transCount = radio.TransceiversCount();
		if (channelIndex < 0 || channelIndex >= transCount)
			return "";

		BaseTransceiver trans = radio.GetTransceiver(channelIndex);
		if (!trans)
			return "";

		int freq = trans.GetFrequency();
		string txt = GetChannelText(channelIndex);

		string suffix = "";
		if (txt != "")
			suffix = string.Format(" (%1)", txt);

		return string.Format("%1: %2 kHz%3", channelIndex + 1, freq, suffix);
	}
}
