// -----------------------------------------------------------------------------
// OETA_ListSpawnConfigComponent.c
// Reforger 1.6+
//
// Holds a LIST of prefabs and exact spawn transform config.
//
//  - Prefabs: array<ResourceName>
//  - Selection mode: Sequential / Random / Fixed index
//  - Exact local X/Y/Z offsets
//  - Exact Yaw / Pitch / Roll rotation (degrees)
//  - Delete previous spawn
//  - Debug toggle
//
// Action calls SpawnFromList().
// -----------------------------------------------------------------------------

class OETA_ListSpawnConfigComponentClass : ScriptComponentClass {}

class OETA_ListSpawnConfigComponent : ScriptComponent
{
	// ---------------- PREFAB LIST ----------------

	[Attribute("", UIWidgets.ResourceNamePicker,
		"Prefab list",
		"Add multiple prefabs to spawn from")]
	protected ref array<ResourceName> m_PrefabList;

	// 0 = Sequential, 1 = Random, 2 = Fixed index
	[Attribute("0", UIWidgets.ComboBox,
		"Selection mode",
		"0 = Sequential, 1 = Random, 2 = Fixed index")]
	protected int m_SelectionMode;

	[Attribute("0", UIWidgets.EditBox,
		"Fixed index",
		"Used when selection mode = Fixed index (0-based)")]
	protected int m_FixedIndex;


	// ---------------- POSITION ----------------

	[Attribute("0", UIWidgets.EditBox,
		"Local X",
		"Local X offset relative to owner")]
	protected float m_LocalPosX;

	[Attribute("0", UIWidgets.EditBox,
		"Local Y",
		"Local Y offset relative to owner")]
	protected float m_LocalPosY;

	[Attribute("5", UIWidgets.EditBox,
		"Local Z",
		"Local Z offset relative to owner")]
	protected float m_LocalPosZ;


	// ---------------- ROTATION ----------------

	[Attribute("0", UIWidgets.EditBox,
		"Yaw (deg)",
		"Rotation around vertical axis")]
	protected float m_Yaw;

	[Attribute("0", UIWidgets.EditBox,
		"Pitch (deg)",
		"Rotation around right axis")]
	protected float m_Pitch;

	[Attribute("0", UIWidgets.EditBox,
		"Roll (deg)",
		"Rotation around forward axis")]
	protected float m_Roll;


	// ---------------- FLAGS ----------------

	[Attribute("1", UIWidgets.CheckBox,
		"Delete previous spawn",
		"If true, removes previously spawned entity")]
	protected bool m_DeletePrevious;

	[Attribute("0", UIWidgets.CheckBox,
		"Debug",
		"Enable debug output")]
	protected bool m_Debug;


	// ---------------- RUNTIME ----------------

	protected IEntity m_LastSpawned;
	protected int m_SequentialIndex;


	// -------------------------------------------------------------------------
	protected ResourceName SelectPrefab()
	{
		if (!m_PrefabList || m_PrefabList.IsEmpty())
		{
			if (m_Debug)
				Print("Prefab list is empty!", LogLevel.WARNING);
			return "";
		}

		int count = m_PrefabList.Count();
		int index = 0;

		switch (m_SelectionMode)
		{
			case 0: // Sequential
				index = m_SequentialIndex % count;
				m_SequentialIndex = (m_SequentialIndex + 1) % count;
				break;

			case 1: // Random
				index = Math.RandomInt(0, count);
				break;

			case 2: // Fixed
				index = Math.Clamp(m_FixedIndex, 0, count - 1);
				break;
		}

		ResourceName result = m_PrefabList.Get(index);

		if (m_Debug)
			Print(string.Format("SelectPrefab → index=%1 prefab=%2", index, result));

		return result;
	}

	// -------------------------------------------------------------------------
	IEntity SpawnFromList(IEntity user = null)
	{
		IEntity owner = GetOwner();
		if (!owner)
		{
			if (m_Debug)
				Print("No owner entity!", LogLevel.ERROR);
			return null;
		}

		// Choose prefab
		ResourceName prefabName = SelectPrefab();
		if (!prefabName || prefabName == "")
			return null;

		// Load resource
		Resource res = Resource.Load(prefabName);
		if (!res)
		{
			if (m_Debug)
				Print(string.Format("Failed to load '%1'", prefabName), LogLevel.ERROR);
			return null;
		}

		// Delete previous entity
		if (m_DeletePrevious && m_LastSpawned)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_LastSpawned);
			m_LastSpawned = null;
		}

		// Get owner transform
		vector ownerMat[4];
		owner.GetWorldTransform(ownerMat);

		// Compute world position = owner + local offset
		vector localPos = Vector(m_LocalPosX, m_LocalPosY, m_LocalPosZ);

		vector worldPos =
			ownerMat[3]
			+ ownerMat[0] * localPos[0]
			+ ownerMat[1] * localPos[1]
			+ ownerMat[2] * localPos[2];

		// Build rotation from Euler
		vector angles = Vector(m_Yaw, m_Pitch, m_Roll);
		vector rotMat[3];
		Math3D.AnglesToMatrix(angles, rotMat);

		// Construct final transform
		vector spawnMat[4];
		spawnMat[0] = rotMat[0];
		spawnMat[1] = rotMat[1];
		spawnMat[2] = rotMat[2];
		spawnMat[3] = worldPos;

		// Spawn entity
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = spawnMat;

		IEntity spawned = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!spawned)
		{
			if (m_Debug)
				Print("Spawn failed!", LogLevel.ERROR);
			return null;
		}

		m_LastSpawned = spawned;

		if (m_Debug)
			Print(string.Format("Spawned '%1' successfully", prefabName));

		return spawned;
	}
}
