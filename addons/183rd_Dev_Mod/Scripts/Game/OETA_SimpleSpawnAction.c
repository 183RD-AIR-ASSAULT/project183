// -----------------------------------------------------------------------------
// OETA_SpawnAtNamedPointAction.c
// Reforger 1.6+
//
// Spawns a prefab at the WORLD transform of a named entity,
// similar to the fast travel script using destination names.
//
//  - PrefabToSpawn: .et prefab to spawn
//  - SpawnPointName: name of entity whose transform we use
//  - HeightOffset: lift spawn a bit to avoid clipping
//  - DeletePrevious: despawn last entity from THIS action
// -----------------------------------------------------------------------------

class OETA_SpawnAtNamedPointAction : ScriptedUserAction
{
	// Prefab to spawn
	[Attribute("", UIWidgets.ResourceNamePicker,
		"Prefab to spawn",
		"Prefab to spawn at the named point")]
	protected ResourceName m_PrefabToSpawn;

	// Name of the entity to use as spawn location (like fast travel destinations)
	[Attribute("", UIWidgets.EditBox,
		"Spawn point name",
		"Name of entity whose world transform is used for spawn")]
	protected string m_SpawnPointName;

	// Vertical offset to avoid exact floor clipping
	[Attribute("0.5", UIWidgets.EditBox,
		"Height offset",
		"Extra height added to spawn position (meters)")]
	protected float m_HeightOffset;

	// Delete previously spawned entity from THIS action
	[Attribute("1", UIWidgets.CheckBox,
		"Delete previous spawn",
		"Delete last spawned entity before spawning a new one")]
	protected bool m_DeletePrevious;

	// Debug logging
	[Attribute("0", UIWidgets.CheckBox,
		"Debug",
		"Enable debug output")]
	protected bool m_Debug;

	protected IEntity m_LastSpawned;

	// -------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		// Add GM / faction checks later if you want
		return true;
	}

	// -------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity)
		{
			if (m_Debug)
				Print("OETA_SpawnAtNamedPointAction::PerformAction - no owner!", LogLevel.ERROR);
			return;
		}

		if (!m_PrefabToSpawn || m_PrefabToSpawn == "")
		{
			if (m_Debug)
				Print("OETA_SpawnAtNamedPointAction::PerformAction - prefab not set!", LogLevel.ERROR);
			return;
		}

		if (m_SpawnPointName == "")
		{
			if (m_Debug)
				Print("OETA_SpawnAtNamedPointAction::PerformAction - spawn point name is empty!", LogLevel.ERROR);
			return;
		}

		// Find the spawn point by name (like fast travel destinations)
		World world = GetGame().GetWorld();
		if (!world)
		{
			if (m_Debug)
				Print("OETA_SpawnAtNamedPointAction::PerformAction - no world!", LogLevel.ERROR);
			return;
		}

		IEntity spawnPoint = world.FindEntityByName(m_SpawnPointName);
		if (!spawnPoint)
		{
			if (m_Debug)
				Print(string.Format("Spawn point '%1' not found!", m_SpawnPointName), LogLevel.ERROR);
			return;
		}

		// Load prefab
		Resource res = Resource.Load(m_PrefabToSpawn);
		if (!res)
		{
			if (m_Debug)
				Print(string.Format("Failed to load prefab '%1'", m_PrefabToSpawn), LogLevel.ERROR);
			return;
		}

		// Delete previous spawn if requested
		if (m_DeletePrevious && m_LastSpawned)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_LastSpawned);
			m_LastSpawned = null;
		}

		// Get spawn point transform
		vector mat[4];
		spawnPoint.GetWorldTransform(mat);

		// Apply height offset
		mat[3][1] = mat[3][1] + m_HeightOffset;

		// Spawn entity
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = mat;

		IEntity spawned = GetGame().SpawnEntityPrefab(res, world, params);
		if (!spawned)
		{
			if (m_Debug)
				Print(string.Format("Failed to spawn '%1' at '%2'", m_PrefabToSpawn, m_SpawnPointName), LogLevel.ERROR);
			return;
		}

		m_LastSpawned = spawned;

		if (m_Debug)
		{
			string uname = "Unknown";
			SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(pUserEntity);
			if (ch)
				uname = ch.GetName();

			Print(string.Format(
				"Spawned '%1' for '%2' at point '%3'",
				m_PrefabToSpawn, uname, m_SpawnPointName));
		}
	}
}
