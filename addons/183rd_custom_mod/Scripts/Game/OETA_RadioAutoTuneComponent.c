// -----------------------------------------------------------------------------
// OETA_RadioAutoTuneComponent.c
// Reforger 1.6 – player-based channel text, config-driven channel plans
//
// Personal radios:
//   - Channel index 0 -> Player's group frequency
//   - Channel index 1 -> Faction frequency
//   - Channels starting at m_ChannelPlanStartIndex (default 2 => Ch3–8)
//     can be filled by a channel plan based on group frequency.
//   - Plan auto-selected by matching group's freq to BaseFrequenciesKHz values.
//
// Manpacks / vehicle radios:
//   - If m_OnlyPersonalForAutoTune = true, they are NOT auto-tuned by
//     group/faction.
//   - Instead, attach OETA_RadioPlanOverrideComponent to the radio owner and
//     pick a plan by name (BaseFrequenciesKHz can be empty for override-only).
//   - Override plans start at channel 1 (index 0) by default.
//
// Plans (.conf):
//   - Each OETA_RadioPlanConfig has:
//       string BaseFrequenciesKHz  (space-separated list; empty = no auto-match)
//       string Name                (plan ID)
//       Slots[]                    (frequencies + labels)
//   - To let multiple squad freqs share the same plan:
//       put them all in BaseFrequenciesKHz, e.g. "68500 69500 70500".
//
// Channel text:
//   - Stored in OETA_RadioChannelNamesComponent on the same player entity.
//   - Only channels that have entries in the chosen plan are modified.
// -----------------------------------------------------------------------------

// ============================================================================
// Runtime plan model
// ============================================================================
class OETA_RadioChannelEntry
{
	int    m_FrequencyKHz;
	string m_Text;

	void OETA_RadioChannelEntry(int freqKHz, string text)
	{
		m_FrequencyKHz = freqKHz;
		m_Text         = text;
	}
}

class OETA_RadioChannelPlan
{
	ref array<int> m_BaseFrequenciesKHz;
	string m_Name;
	protected ref array<ref OETA_RadioChannelEntry> m_Entries;

	void OETA_RadioChannelPlan(string name)
	{
		m_Name               = name;
		m_BaseFrequenciesKHz = new array<int>();
		m_Entries            = new array<ref OETA_RadioChannelEntry>();
	}

	void AddBaseFrequency(int freqKHz)
	{
		if (freqKHz <= 0)
			return;

		if (!m_BaseFrequenciesKHz)
			m_BaseFrequenciesKHz = new array<int>();

		foreach (int f : m_BaseFrequenciesKHz)
		{
			if (f == freqKHz)
				return;
		}
		m_BaseFrequenciesKHz.Insert(freqKHz);
	}

	void Add(int freqKHz, string text)
	{
		if (!m_Entries)
			m_Entries = new array<ref OETA_RadioChannelEntry>();

		m_Entries.Insert(new OETA_RadioChannelEntry(freqKHz, text));
	}

	void AddEmpty()
	{
		Add(0, "");
	}

	bool Matches(int baseFreqKHz)
	{
		if (!m_BaseFrequenciesKHz || m_BaseFrequenciesKHz.IsEmpty())
			return false;

		foreach (int f : m_BaseFrequenciesKHz)
		{
			if (f == baseFreqKHz)
				return true;
		}
		return false;
	}

	int GetEntryCount()
	{
		if (!m_Entries)
			return 0;
		return m_Entries.Count();
	}

	void ApplyToRadio(BaseRadioComponent radio, OETA_RadioChannelNamesComponent namesComp,
		int startChannelIndex, bool debugLog)
	{
		if (!radio || !m_Entries)
			return;

		int transCount = radio.TransceiversCount();
		int numEntries = m_Entries.Count();

		for (int i = 0; i < numEntries; i++)
		{
			int chIndex = startChannelIndex + i;
			if (chIndex >= transCount)
				break;

			OETA_RadioChannelEntry entry = m_Entries[i];
			if (!entry)
				continue;

			bool hasFreq = entry.m_FrequencyKHz > 0;
			bool hasText = entry.m_Text != "";

			// Pure empty slot: leave channel as-is.
			if (!hasFreq && !hasText)
				continue;

			BaseTransceiver ch = radio.GetTransceiver(chIndex);
			if (!ch)
				continue;

			if (hasFreq)
			{
				int freq = entry.m_FrequencyKHz;
				int fMin = ch.GetMinFrequency();
				int fMax = ch.GetMaxFrequency();
				if (freq < fMin) freq = fMin;
				if (freq > fMax) freq = fMax;

				ch.SetFrequency(freq);

				if (debugLog)
					PrintFormat("[OETA_RadioAutoTune] Plan '%4' ch%1 -> %2 kHz (%3)",
						chIndex, freq, entry.m_Text, m_Name);
			}

			if (namesComp && hasText)
				namesComp.SetChannelText(chIndex, entry.m_Text);
		}
	}
}

// ============================================================================
// Main auto-tune component
// ============================================================================
class OETA_RadioAutoTuneComponentClass : ScriptComponentClass {}

[ComponentEditorProps(category: "Radio", description: "Auto-tunes radios to group & faction frequencies")]
class OETA_RadioAutoTuneComponent : ScriptComponent
{
	// Channels (zero-based, 0 = Channel 1)
	[Attribute(defvalue: "0", uiwidget: UIWidgets.Slider,
		desc: "Group channel index", params: "0 3 1")]
	protected int m_GroupChannelIndex;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Slider,
		desc: "Faction channel index", params: "0 3 1")]
	protected int m_FactionChannelIndex;

	// Plan application for personal radios
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox,
		desc: "Apply channel plan based on group freq (personal radios)")]
	protected bool m_EnableChannelPlans;

	// Default 2 => Channel 3 as first plan channel
	[Attribute(defvalue: "2", uiwidget: UIWidgets.Slider,
		desc: "Channel plan start index", params: "0 15 1")]
	protected int m_ChannelPlanStartIndex;

	// Encryption
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox,
		desc: "Sync encryption key to faction when faction retunes")]
	protected bool m_SyncFactionEncryption;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox,
		desc: "Prefer group encryption key after group retune")]
	protected bool m_PreferGroupEncryption;

	// Group freq override
	[Attribute(defvalue: "0", uiwidget: UIWidgets.EditBox,
		desc: "Group Freq Override (kHz). 0 = disabled")]
	protected int m_GroupFreqOverrideKHz;

	// Plan config
	[Attribute("", uiwidget: UIWidgets.Object,
		desc: "Radio channel plan config (OETA_RadioPlanConfigRoot)")]
	protected ref OETA_RadioPlanConfigRoot m_PlanConfig;

	// If true: group/faction + auto plans are only applied to PERSONAL radios.
	// Manpacks / vehicle radios will only be tuned via OETA_RadioPlanOverrideComponent.
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox,
		desc: "Use group/faction auto-tune only on PERSONAL radios")]
	protected bool m_OnlyPersonalForAutoTune;

	// Debug
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Verbose logging")]
	protected bool m_Debug;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox,
		desc: "Run on clients (for testing)")]
	protected bool m_RunOnClient;

	// State
	protected IEntity m_Owner;
	protected ScriptedInventoryStorageManagerComponent m_InvMgr;
	protected InventoryStorageManagerComponent m_InvMgrBase;

	protected int m_FactionRetry = 0;
	protected const int MAX_FACTION_RETRY = 20;

	static ref array<ref OETA_RadioChannelPlan> s_Plans;

	// ----------------------------------------------------------------------
	// Small helper: split by whitespace into tokens (no Split()/ParseString)
	// ----------------------------------------------------------------------
	protected void SplitWhitespace(string src, array<string> outTokens)
	{
		if (!outTokens)
			return;

		outTokens.Clear();

		int len = src.Length();
		string current = "";

		for (int i = 0; i < len; i++)
		{
			string ch = src.Substring(i, 1);

			// treat whitespace as separator
			if (ch == " " || ch == "\t" || ch == "\n" || ch == "\r")
			{
				if (current != "")
				{
					outTokens.Insert(current);
					current = "";
				}
			}
			else
			{
				current += ch;
			}
		}

		if (current != "")
			outTokens.Insert(current);
	}

	// ----------------------------------------------------------------------
	// Init / plan loading
	// ----------------------------------------------------------------------
	protected void InitPlansFromConfig()
	{
		if (s_Plans)
			return;

		s_Plans = new array<ref OETA_RadioChannelPlan>();

		if (!m_PlanConfig || !m_PlanConfig.Plans || m_PlanConfig.Plans.IsEmpty())
		{
			if (m_Debug)
				Print("[OETA_RadioAutoTune] No plan config; channel plans disabled.");
			return;
		}

		foreach (OETA_RadioPlanConfig pc : m_PlanConfig.Plans)
		{
			if (!pc)
				continue;

			string pname = pc.Name;
			pname = pname.Trim();
			if (pname == "")
				continue;

			// Find or create runtime plan by Name
			OETA_RadioChannelPlan plan = FindPlanByName(pname);
			if (!plan)
			{
				plan = new OETA_RadioChannelPlan(pname);

				// First definition with this Name carries the slot layout
				if (pc.Slots)
				{
					foreach (OETA_RadioPlanSlotConfig slot : pc.Slots)
					{
						if (!slot)
							continue;

						if (slot.FrequencyKHz == 0 && slot.Text == "")
							plan.AddEmpty(); // "skip this channel"
						else
							plan.Add(slot.FrequencyKHz, slot.Text);
					}
				}

				s_Plans.Insert(plan);
			}
			// For subsequent configs with same Name, we ignore Slots and only
			// aggregate BaseFrequenciesKHz.

			// Parse space-separated list of base freqs (if any)
			string list = pc.BaseFrequenciesKHz;
			list = list.Trim();
			if (list != "")
			{
				array<string> tokens = {};
				SplitWhitespace(list, tokens);

				foreach (string t : tokens)
				{
					t = t.Trim();
					if (t == "")
						continue;

					int freq = t.ToInt();
					if (freq > 0)
						plan.AddBaseFrequency(freq);
				}
			}
		}

		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] Loaded %1 runtime plans from config", s_Plans.Count());
	}

	protected OETA_RadioChannelNamesComponent FindNamesComponent()
	{
		if (!m_Owner)
			return null;

		OETA_RadioChannelNamesComponent n = OETA_RadioChannelNamesComponent.Cast(
			m_Owner.FindComponent(OETA_RadioChannelNamesComponent)
		);
		if (n) return n;

		IEntity p = m_Owner.GetParent();
		while (p)
		{
			n = OETA_RadioChannelNamesComponent.Cast(p.FindComponent(OETA_RadioChannelNamesComponent));
			if (n) return n;
			p = p.GetParent();
		}

		IEntity c = m_Owner.GetChildren();
		while (c)
		{
			n = OETA_RadioChannelNamesComponent.Cast(c.FindComponent(OETA_RadioChannelNamesComponent));
			if (n) return n;
			c = c.GetSibling();
		}
		return null;
	}

	protected OETA_RadioChannelPlan FindPlanByName(string planName)
	{
		if (!s_Plans)
			return null;

		foreach (OETA_RadioChannelPlan plan : s_Plans)
		{
			if (!plan)
				continue;
			if (plan.m_Name == planName)
				return plan;
		}
		return null;
	}

	// ----------------------------------------------------------------------
	// Radio classification
	// ----------------------------------------------------------------------
	// Is this radio considered "personal" (not backpack) based on gadget type?
	protected bool IsPersonalRadio(BaseRadioComponent radio)
	{
		if (!radio)
			return false;

		IEntity owner = radio.GetOwner();
		if (!owner)
			return false;

		SCR_RadioComponent scrRadio = SCR_RadioComponent.Cast(
			owner.FindComponent(SCR_RadioComponent)
		);
		if (!scrRadio)
			return false;

		EGadgetType gType = scrRadio.GetType();

		// RADIO_BACKPACK -> treat as manpack (non-personal)
		if (gType == EGadgetType.RADIO_BACKPACK)
			return false;

		// All other radio types count as "personal"
		return true;
	}

	// ----------------------------------------------------------------------
	// Auto plan selection (for personal radios, based on group freq)
	// ----------------------------------------------------------------------
	protected void ApplyChannelPlan(BaseRadioComponent radio, int baseFreqKHz)
	{
		if (!m_EnableChannelPlans)
			return;

		InitPlansFromConfig();
		if (!s_Plans || s_Plans.Count() == 0)
			return;

		OETA_RadioChannelNamesComponent names = FindNamesComponent();

		foreach (OETA_RadioChannelPlan plan : s_Plans)
		{
			if (!plan || !plan.Matches(baseFreqKHz))
				continue;

			int entryCount = plan.GetEntryCount();

			// Only clear text for channels actually defined in this plan.
			if (names && entryCount > 0)
				names.ClearTextRange(m_ChannelPlanStartIndex, entryCount);

			if (m_Debug)
				PrintFormat("[OETA_RadioAutoTune] Applying plan '%2' for base freq %1 kHz",
					baseFreqKHz, plan.m_Name);

			// Only channels with entries in the plan are touched.
			plan.ApplyToRadio(radio, names, m_ChannelPlanStartIndex, m_Debug);

			if (m_Debug && names)
			{
				for (int i = 0; i < entryCount; i++)
				{
					int chIndex = m_ChannelPlanStartIndex + i;
					string t = names.GetChannelText(chIndex);
					PrintFormat("[OETA_RadioAutoTune] Text debug ch%1 = '%2'", chIndex, t);
				}
			}
			return;
		}

		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] No matching channel plan for base freq %1 kHz",
				baseFreqKHz);
	}

	// ----------------------------------------------------------------------
	// Plan override (for manpacks / vehicle radios, or anything with component)
	// ----------------------------------------------------------------------
	protected bool ApplyOverridePlanIfPresent(BaseRadioComponent radio)
	{
		if (!radio)
			return false;

		IEntity owner = radio.GetOwner();
		if (!owner)
			return false;

		OETA_RadioPlanOverrideComponent overrideComp = OETA_RadioPlanOverrideComponent.Cast(
			owner.FindComponent(OETA_RadioPlanOverrideComponent)
		);
		if (!overrideComp)
			return false;

		string planName = overrideComp.GetPlanName();
		planName = planName.Trim();
		if (planName == "")
			return false;

		InitPlansFromConfig();
		if (!s_Plans)
			return false;

		OETA_RadioChannelPlan plan = FindPlanByName(planName);
		if (!plan)
		{
			if (m_Debug)
				PrintFormat("[OETA_RadioAutoTune] Override plan '%1' not found", planName);
			return false;
		}

		int entryCount = plan.GetEntryCount();
		if (entryCount <= 0)
			return false;

		// Default: start at channel 1 (index 0) for override plans (manpacks / vehicles).
		int startIndex = 0;
		int overrideIndex = overrideComp.GetStartChannelIndexOverride();
		if (overrideIndex >= 0)
			startIndex = overrideIndex;

		OETA_RadioChannelNamesComponent names = FindNamesComponent();

		// Only clear text for the channels this override will touch
		if (names)
			names.ClearTextRange(startIndex, entryCount);

		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] Applying override plan '%1' at start index %2",
				planName, startIndex);

		plan.ApplyToRadio(radio, names, startIndex, m_Debug);

		if (m_Debug && names)
		{
			for (int i = 0; i < entryCount; i++)
			{
				int chIndex = startIndex + i;
				string t = names.GetChannelText(chIndex);
				PrintFormat("[OETA_RadioAutoTune] Override text debug ch%1 = '%2'", chIndex, t);
			}
		}

		return true;
	}

	// ----------------------------------------------------------------------
	// Inventory hookup
	// ----------------------------------------------------------------------
	protected void LocateInvMgrOn(IEntity ent)
	{
		if (!ent || m_InvMgr)
			return;

		m_InvMgr = ScriptedInventoryStorageManagerComponent.Cast(
			ent.FindComponent(ScriptedInventoryStorageManagerComponent)
		);
	}

	protected void ScanChildrenForInvMgr(IEntity ent)
	{
		IEntity c = ent.GetChildren();
		while (!m_InvMgr && c)
		{
			LocateInvMgrOn(c);
			c = c.GetSibling();
		}
	}

	protected void LocateInvMgr()
	{
		LocateInvMgrOn(m_Owner);
		if (!m_InvMgr)
			ScanChildrenForInvMgr(m_Owner);

		m_InvMgrBase = InventoryStorageManagerComponent.Cast(m_InvMgr);

		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] LocateInvMgr -> inv=%1 base=%2",
				m_InvMgr, m_InvMgrBase);
	}

	// ----------------------------------------------------------------------
	// Faction resolve & global retune
	// ----------------------------------------------------------------------
	protected void WaitForFactionThenRetune()
	{
		SCR_Faction fac = ResolveFactionOnce();
		if (fac)
		{
			if (m_Debug)
				PrintFormat("[OETA_RadioAutoTune] Faction resolved: %1", fac);
			RetuneAllRadios();
			return;
		}

		if (m_FactionRetry < MAX_FACTION_RETRY)
		{
			m_FactionRetry++;
			if (m_Debug)
				PrintFormat("[OETA_RadioAutoTune] ResolveFaction failed (try %1/%2) — retrying...",
					m_FactionRetry, MAX_FACTION_RETRY);
			GetGame().GetCallqueue().CallLater(WaitForFactionThenRetune, 200, false);
		}
		else if (m_Debug)
			Print("[OETA_RadioAutoTune] ResolveFaction failed — giving up");
	}

	protected void RetuneAllRadios()
	{
		if (!m_InvMgrBase)
			return;

		array<IEntity> withComps = {};
		array<typename> want = { BaseRadioComponent };
		m_InvMgrBase.FindItemsWithComponents(withComps, want);

		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] RetuneAllRadios count=%1", withComps.Count());

		foreach (IEntity it : withComps)
		{
			BaseRadioComponent radio = BaseRadioComponent.Cast(it.FindComponent(BaseRadioComponent));
			if (radio)
				RetuneRadioBoth(radio);
		}
	}

	// ----------------------------------------------------------------------
	// Core retune for a single radio
	// ----------------------------------------------------------------------
	protected void RetuneRadioBoth(BaseRadioComponent radio)
	{
		if (!radio)
			return;

		bool isPersonal = IsPersonalRadio(radio);

		// PERSONAL radios: group/faction + auto plan (if allowed)
		if (!m_OnlyPersonalForAutoTune || isPersonal)
		{
			OETA_RadioChannelNamesComponent names = FindNamesComponent();
			int groupFreqApplied = 0;

			// GROUP
			if (radio.TransceiversCount() > m_GroupChannelIndex)
			{
				BaseTransceiver chG = radio.GetTransceiver(m_GroupChannelIndex);
				if (chG)
				{
					int gFreq; string gKey;
					if (ResolveGroupRadio(gFreq, gKey))
					{
						int gMin = chG.GetMinFrequency();
						int gMax = chG.GetMaxFrequency();
						if (gFreq < gMin) gFreq = gMin;
						if (gFreq > gMax) gFreq = gMax;

						chG.SetFrequency(gFreq);
						groupFreqApplied = gFreq;

						if (m_PreferGroupEncryption && gKey != "")
							radio.SetEncryptionKey(gKey);

						if (names)
							names.SetChannelText(m_GroupChannelIndex, "Group");

						if (m_Debug)
							PrintFormat("[OETA_RadioAutoTune] Group ch%1 -> %2 kHz",
								m_GroupChannelIndex, gFreq);
					}
					else if (m_Debug)
						Print("[OETA_RadioAutoTune] No group frequency found");
				}
			}
			else if (m_Debug)
			{
				PrintFormat("[OETA_RadioAutoTune] Radio has %1 transceivers; group index %2 invalid",
					radio.TransceiversCount(), m_GroupChannelIndex);
			}

			// PLAN (based on group freq) – only channels with entries in the plan get touched.
			if (groupFreqApplied > 0)
				ApplyChannelPlan(radio, groupFreqApplied);

			// FACTION
			if (radio.TransceiversCount() > m_FactionChannelIndex)
			{
				BaseTransceiver chF = radio.GetTransceiver(m_FactionChannelIndex);
				if (chF)
				{
					SCR_Faction fac = ResolveFactionOnce();
					if (fac)
					{
						int fFreq = fac.GetFactionRadioFrequency();
						int fMin = chF.GetMinFrequency();
						int fMax = chF.GetMaxFrequency();
						if (fFreq < fMin) fFreq = fMin;
						if (fFreq > fMax) fFreq = fMax;

						chF.SetFrequency(fFreq);

						if (m_SyncFactionEncryption)
							radio.SetEncryptionKey(fac.GetFactionRadioEncryptionKey());

						if (names)
							names.SetChannelText(m_FactionChannelIndex, "Faction");

						if (m_Debug)
							PrintFormat("[OETA_RadioAutoTune] Faction ch%1 -> %2 kHz",
								m_FactionChannelIndex, fFreq);
					}
					else if (m_Debug)
						Print("[OETA_RadioAutoTune] ResolveFaction failed inside RetuneRadioBoth");
				}
			}
			else if (m_Debug)
			{
				PrintFormat("[OETA_RadioAutoTune] Radio has %1 transceivers; faction index %2 invalid",
					radio.TransceiversCount(), m_FactionChannelIndex);
			}
		}

		// Override plans can apply to ANY radio (personal or not) if the component is present
		ApplyOverridePlanIfPresent(radio);
	}

	// ----------------------------------------------------------------------
	// Group frequency resolution
	// ----------------------------------------------------------------------
	protected bool ResolveGroupRadio(out int freqKHz, out string key)
	{
		// 1) Explicit override
		if (m_GroupFreqOverrideKHz > 0)
		{
			freqKHz = m_GroupFreqOverrideKHz;
			key = "";
			return true;
		}

		// 2) Player's current group via GroupsManager
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			int pid = pm.GetPlayerIdFromControlledEntity(m_Owner);
			if (pid != 0)
			{
				SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
				if (gm)
				{
					SCR_AIGroup group = gm.GetPlayerGroup(pid);
					if (group)
					{
						int liveFreq = group.GetRadioFrequency();
						if (m_Debug)
							PrintFormat("[OETA_RadioAutoTune] Group from GM: id=%1 freq=%2",
								group.GetGroupID(), liveFreq);
						if (liveFreq > 0)
						{
							freqKHz = liveFreq;
							key = "";
							return true;
						}
					}
				}
			}
		}

		// 3) Fallback: OETA_GroupRadioSettingsComponent
		OETA_GroupRadioSettingsComponent g = FindGroupSettings();
		if (g && g.GetGroupFrequencyKHz() > 0)
		{
			freqKHz = g.GetGroupFrequencyKHz();
			if (!g.HasGroupEncryptionKey(key))
				key = "";
			return true;
		}

		freqKHz = 0;
		key = "";
		return false;
	}

	protected OETA_GroupRadioSettingsComponent FindGroupSettings()
	{
		OETA_GroupRadioSettingsComponent g = OETA_GroupRadioSettingsComponent.Cast(
			m_Owner.FindComponent(OETA_GroupRadioSettingsComponent)
		);
		if (g) return g;

		IEntity p = m_Owner.GetParent();
		while (p)
		{
			g = OETA_GroupRadioSettingsComponent.Cast(p.FindComponent(OETA_GroupRadioSettingsComponent));
			if (g) return g;
			p = p.GetParent();
		}

		IEntity c = m_Owner.GetChildren();
		while (c)
		{
			g = OETA_GroupRadioSettingsComponent.Cast(
				c.FindComponent(OETA_GroupRadioSettingsComponent)
			);
			if (g) return g;
			c = c.GetSibling();
		}
		return null;
	}

	// ----------------------------------------------------------------------
	// Faction helpers
	// ----------------------------------------------------------------------
	protected SCR_FactionAffiliationComponent FindAffiliationComp()
	{
		SCR_FactionAffiliationComponent aff = SCR_FactionAffiliationComponent.Cast(
			m_Owner.FindComponent(SCR_FactionAffiliationComponent)
		);
		if (aff) return aff;

		IEntity p = m_Owner.GetParent();
		while (p)
		{
			aff = SCR_FactionAffiliationComponent.Cast(
				p.FindComponent(SCR_FactionAffiliationComponent)
			);
			if (aff) return aff;
			p = p.GetParent();
		}

		IEntity c = m_Owner.GetChildren();
		while (c)
		{
			aff = SCR_FactionAffiliationComponent.Cast(
				c.FindComponent(SCR_FactionAffiliationComponent)
			);
			if (aff) return aff;
			c = c.GetSibling();
		}

		return null;
	}

	protected SCR_Faction ResolveFactionOnce()
	{
		SCR_FactionAffiliationComponent aff = FindAffiliationComp();
		if (aff)
		{
			Faction f = aff.GetAffiliatedFaction();
			if (f)
			{
				SCR_Faction sf = SCR_Faction.Cast(f);
				if (sf) return sf;
			}
		}

		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			int pid = pm.GetPlayerIdFromControlledEntity(m_Owner);
			if (pid != 0)
			{
				SCR_Faction pf = SCR_Faction.Cast(SCR_FactionManager.SGetPlayerFaction(pid));
				if (pf) return pf;
			}
		}
		return null;
	}

	// ----------------------------------------------------------------------
	// Lifecycle / hooks
	// ----------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;

		if (m_Debug)
			Print("[OETA_RadioAutoTune] OnPostInit");

		if (!Replication.IsServer() && !m_RunOnClient)
		{
			if (m_Debug)
				Print("[OETA_RadioAutoTune] Not server; skipping (enable Run on clients to test)");
			return;
		}

		InitPlansFromConfig();
		LocateInvMgr();
		GetGame().GetCallqueue().CallLater(DeferredHook, 50, false);
	}

	override protected void OnDelete(IEntity owner)
	{
		if (m_InvMgr && m_InvMgr.m_OnItemAddedInvoker)
			m_InvMgr.m_OnItemAddedInvoker.Remove(OnItemAdded);

		super.OnDelete(owner);
	}

	protected void DeferredHook()
	{
		if (m_Debug)
			Print("[OETA_RadioAutoTune] DeferredHook");

		LocateInvMgr();
		if (m_InvMgr && m_InvMgr.m_OnItemAddedInvoker)
		{
			m_InvMgr.m_OnItemAddedInvoker.Insert(OnItemAdded);
			if (m_Debug)
				Print("[OETA_RadioAutoTune] Subscribed to m_OnItemAddedInvoker");
		}
		else if (m_Debug)
			Print("[OETA_RadioAutoTune] Inventory manager / invoker not found");

		WaitForFactionThenRetune();
	}

	void OnItemAdded(IEntity item, BaseInventoryStorageComponent storage)
	{
		if (m_Debug)
			PrintFormat("[OETA_RadioAutoTune] OnItemAdded item=%1 storage=%2", item, storage);

		if (!item)
			return;

		BaseRadioComponent radio = BaseRadioComponent.Cast(item.FindComponent(BaseRadioComponent));
		if (radio)
			RetuneRadioBoth(radio);
	}

	// Manual trigger
	void ForceRetune()
	{
		if (!Replication.IsServer() && !m_RunOnClient)
			return;

		if (m_Debug)
			Print("[OETA_RadioAutoTune] ForceRetune");

		RetuneAllRadios();
	}
}
