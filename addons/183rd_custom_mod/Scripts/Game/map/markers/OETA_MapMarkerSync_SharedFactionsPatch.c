modded class SCR_MapMarkerSyncComponent
{
	// ------------------------------------------------------------
	// Config lookup (from GameMode)
	// ------------------------------------------------------------
	protected OETA_MarkerShareConfigComponent OETA_GetConfig()
	{
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gm)
			return null;

		return OETA_MarkerShareConfigComponent.Cast(
			gm.FindComponent(OETA_MarkerShareConfigComponent)
		);
	}

	// ------------------------------------------------------------
	// Build faction bitmask from CSV list of faction KEYS
	// ------------------------------------------------------------
	protected int OETA_FactionMaskFromKeys(string keysCsv)
	{
		if (keysCsv == string.Empty)
			return 0;

		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!fm)
			return 0;

		array<Faction> factions = {};
		fm.GetFactionsList(factions);

		array<string> keys = {};
		string current = "";

		for (int i = 0; i < keysCsv.Length(); i++)
		{
			string ch = keysCsv.Get(i);
			if (ch == ",")
			{
				current.TrimInPlace();
				if (current != string.Empty)
					keys.Insert(current);
				current = "";
				continue;
			}
			current += ch;
		}

		current.TrimInPlace();
		if (current != string.Empty)
			keys.Insert(current);

		int mask = 0;

		foreach (string wantedKey : keys)
		{
			foreach (Faction f : factions)
			{
				if (!f)
					continue;

				if (f.GetFactionKey() == wantedKey)
				{
					int idx = fm.GetFactionIndex(f);
					if (idx >= 0)
						mask |= (1 << idx);
					break;
				}
			}
		}

		return mask;
	}

	// ------------------------------------------------------------
	// OVERRIDE: server RPC for adding static markers
	// ------------------------------------------------------------
	override protected void RPC_AskAddStaticMarker(SCR_MapMarkerBase markerData)
	{
		SCR_MapMarkerManagerComponent markerMgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markerMgr || m_iPlacedMarkerLimit <= 0)
			return;

		if (m_OwnedMarkers.Count() >= m_iPlacedMarkerLimit)
			Rpc(RPC_AskRemoveStaticMarker, m_OwnedMarkers[0]);

		// -------- OETA: apply shared faction visibility --------
		OETA_MarkerShareConfigComponent cfg = OETA_GetConfig();
		if (cfg)
		{
			int shareMask = OETA_FactionMaskFromKeys(cfg.GetSharedFactionKeys());
			if (shareMask != 0)
				markerData.SetMarkerFactionFlags(shareMask);
		}

		// -------- vanilla behavior --------
		int ownerId = -1;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetOwner());
		if (pc)
			ownerId = pc.GetPlayerId();

		markerMgr.AssignMarkerUID(markerData);
		markerData.SetMarkerOwnerID(ownerId);

		m_OwnedMarkers.Insert(markerData.GetMarkerID());

		markerMgr.OnAddSynchedMarker(markerData);
		markerMgr.OnAskAddStaticMarker(markerData);
	}
}
