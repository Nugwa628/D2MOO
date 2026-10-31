#include <D2CommonTestDefines.h>

#ifdef UNITFINDS_TESTS

#include <doctest.h>

#include <Windows.h>

#include <cstdarg>
#include <cstring>
#include <filesystem>
#include <tuple>
#include <utility>
#include <vector>

#include <TestDefinitions.h>
#include <TestUtilities.h>

#include <D2Monsters.h>
#include <DataTbls/LevelsIds.h>
#include <DataTbls/ObjectsIds.h>
#include <Drlg/D2DrlgDrlg.h>
#include <Units/Player.h>
#include <Units/UnitFinds.h>
#include <Units/Units.h>

#include <Fixtures/DataTbls/Fixtures.h>
#include <Fixtures/Units/UnitRoomScene.h>


namespace
{
	struct FindUnitKind
	{
		int nUnitType;
		int nClassId;
		uint32_t dwAnimMode;
		uint32_t dwFlags;
	};

	// Rooms 1 and 3 are in town, room 4 has no adjacent rooms at all
	const std::vector<UnitRoomSceneRoomDesc> find_rooms = {
		{ 100, 100, 40, 40, LEVEL_BLOODMOOR, 0, { 0, 1, 2, 3 } },
		{ 140, 100, 40, 40, LEVEL_ROGUEENCAMPMENT, 0, { 1, 0 } },
		{ 100, 140, 40, 40, LEVEL_COLDPLAINS, 0, { 2, 0, 1 } },
		{ 140, 140, 40, 40, LEVEL_LUTGHOLEIN, 0, { 3, 0 } },
		{ 400, 400, 20, 20, LEVEL_STONYFIELD, 0, {} },
	};
	constexpr int find_room_count = 5;

	template<typename Predicate>
	int find_class_id(int nRecordCount, bool bExpected, const Predicate& predicate)
	{
		for (auto i = 0; i < nRecordCount; ++i)
		{
			if (predicate(i) == bExpected)
			{
				return i;
			}
		}

		return 0;
	}

	// Covers every unit type, dead/dying modes, undead monsters, exploding missiles and the unit flags tested by UNITFINDS_TestUnit
	std::vector<FindUnitKind> make_find_kinds(const D2MonStatsTxt* pMonStatsTxt, int nMonStatsCount, const D2MissilesTxt* pMissilesTxt, int nMissilesCount)
	{
		const auto is_undead = [pMonStatsTxt](int i) { return (pMonStatsTxt[i].dwMonStatsFlags & (MONSTATSFLAG_LUNDEAD | MONSTATSFLAG_HUNDEAD)) != 0; };
		const auto is_explosion = [pMissilesTxt](int i) { return (pMissilesTxt[i].dwMissileFlags & MISSILESFLAG_EXPLOSION) != 0; };

		const int nUndead = find_class_id(nMonStatsCount, true, is_undead);
		const int nNotUndead = find_class_id(nMonStatsCount, false, is_undead);
		const int nExplosion = find_class_id(nMissilesCount, true, is_explosion);
		const int nNoExplosion = find_class_id(nMissilesCount, false, is_explosion);

		return {
			{ UNIT_PLAYER, 0, PLRMODE_NEUTRAL, UNITFLAG_CANBEATTACKED | UNITFLAG_ISVALIDTARGET },
			{ UNIT_PLAYER, 1, PLRMODE_DEAD, UNITFLAG_CANBEATTACKED },
			{ UNIT_PLAYER, 5, PLRMODE_DEATH, UNITFLAG_ISVALIDTARGET },
			{ UNIT_MONSTER, nUndead, MONMODE_NEUTRAL, UNITFLAG_CANBEATTACKED },
			{ UNIT_MONSTER, nUndead, MONMODE_DEAD, UNITFLAG_CANBEATTACKED | UNITFLAG_ISVALIDTARGET },
			{ UNIT_MONSTER, nNotUndead, MONMODE_WALK, UNITFLAG_CANBEATTACKED | UNITFLAG_ISVALIDTARGET },
			{ UNIT_MONSTER, nNotUndead + 1, MONMODE_DEATH, UNITFLAG_ISVALIDTARGET },
			{ UNIT_MONSTER, nNotUndead + 2, MONMODE_DEAD, 0 },
			{ UNIT_OBJECT, 3, OBJMODE_NEUTRAL, UNITFLAG_CANBEATTACKED },
			{ UNIT_MISSILE, nNoExplosion, 0, UNITFLAG_CANBEATTACKED | UNITFLAG_ISVALIDTARGET },
			{ UNIT_MISSILE, nExplosion, 0, UNITFLAG_ISVALIDTARGET },
			{ UNIT_MISSILE, -1, 0, UNITFLAG_CANBEATTACKED },
			{ UNIT_MISSILE, nMissilesCount, 0, 0 },
			{ UNIT_ITEM, 7, 0, UNITFLAG_CANBEATTACKED | UNITFLAG_ISVALIDTARGET },
			{ UNIT_TILE, 0, 0, UNITFLAG_CANBEATTACKED | UNITFLAG_ISVALIDTARGET },
			{ UNIT_TYPES_COUNT, 2, 0, UNITFLAG_CANBEATTACKED | UNITFLAG_ISVALIDTARGET },
		};
	}

	UnitRoomSceneUnitDesc make_unit_at(const FindUnitKind& kind, int nRoom, int nX, int nY)
	{
		UnitRoomSceneUnitDesc desc{ kind.nUnitType, kind.nClassId, nRoom, nX, nY };
		desc.nClientY = nY;
		desc.dwFlags = kind.dwFlags;
		desc.dwAnimMode = kind.dwAnimMode;
		return desc;
	}

	// Places nUnitsPerRoom units in each of the rooms 0-3, the units of a room are linked in reverse order
	std::vector<UnitRoomSceneUnitDesc> make_room_units(const std::vector<FindUnitKind>& kinds, int nUnitsPerRoom)
	{
		std::vector<UnitRoomSceneUnitDesc> units;
		for (auto room_index = 0; room_index < 4; ++room_index)
		{
			const auto& room = find_rooms[room_index];
			for (auto i = 0; i < nUnitsPerRoom; ++i)
			{
				const auto n = room_index * nUnitsPerRoom + i;
				const auto& kind = kinds[(n * 5 + room_index) % kinds.size()];
				units.push_back(make_unit_at(kind, room_index, room.nSubtileX + 2 + (n * 7) % 36, room.nSubtileY + 3 + (n * 11) % 34));
			}
		}
		return units;
	}

	void link_room_units(UnitRoomScene& scene, int nUnitsPerRoom)
	{
		for (auto room_index = 0; room_index < 4; ++room_index)
		{
			std::vector<int> list;
			for (auto i = nUnitsPerRoom - 1; i >= 0; --i)
			{
				list.push_back(room_index * nUnitsPerRoom + i);
			}
			scene.LinkUnitList(room_index, list);
		}
	}

	// Compares the unit and room of the arguments by their scene index instead of following them, the scenes are compared on their own
	void check_unit_find_args_eq(const UnitRoomScene& moo_scene, D2UnitFindArgStrc moo_pUnitFindArg, const UnitRoomScene& original_scene, D2UnitFindArgStrc original_pUnitFindArg)
	{
		CHECK_EQ(moo_scene.IndexOf(moo_pUnitFindArg.pUnit), original_scene.IndexOf(original_pUnitFindArg.pUnit));
		CHECK(moo_pUnitFindArg.pfnUnitTest == original_pUnitFindArg.pfnUnitTest);
		moo_pUnitFindArg.pUnit = nullptr;
		original_pUnitFindArg.pUnit = nullptr;
		MOO_CHECK_EQ(moo_pUnitFindArg, original_pUnitFindArg, "Comparing pUnitFindArg");
	}

	void check_unit_find_data_eq(const UnitRoomScene& moo_scene, D2UnitFindDataStrc moo_pUnitFindData, const UnitRoomScene& original_scene, D2UnitFindDataStrc original_pUnitFindData)
	{
		CHECK_EQ(moo_scene.IndexOf(moo_pUnitFindData.pRoom), original_scene.IndexOf(original_pUnitFindData.pRoom));
		CHECK(moo_pUnitFindData.pfnUnitTest == original_pUnitFindData.pfnUnitTest);
		CHECK_EQ(moo_pUnitFindData.pUnitFindArg == nullptr, original_pUnitFindData.pUnitFindArg == nullptr);
		if (moo_pUnitFindData.pUnitFindArg && original_pUnitFindData.pUnitFindArg)
		{
			check_unit_find_args_eq(moo_scene, *moo_pUnitFindData.pUnitFindArg, original_scene, *original_pUnitFindData.pUnitFindArg);
		}
		moo_pUnitFindData.pRoom = nullptr;
		original_pUnitFindData.pRoom = nullptr;
		moo_pUnitFindData.pUnitFindArg = nullptr;
		original_pUnitFindData.pUnitFindArg = nullptr;
		MOO_CHECK_EQ(moo_pUnitFindData, original_pUnitFindData, "Comparing pUnitFindData");
	}

	// Accepts units whose class id is set in the mask stored in unk0x30 and counts its calls in unk0x28
	int32_t __fastcall test_unit_by_class_mask(D2UnitStrc* pUnit, D2UnitFindArgStrc* pUnitFindArg)
	{
		++pUnitFindArg->unk0x28;
		return (pUnitFindArg->unk0x30 >> (pUnit->dwClassId & 31)) & 1;
	}

	int __fastcall test_nearest_unit_any(D2UnitStrc* pUnit, D2UnitStrc* pSourceUnit)
	{
		return pUnit != pSourceUnit;
	}

	int __fastcall test_nearest_unit_by_class(D2UnitStrc* pUnit, D2UnitStrc* pSourceUnit)
	{
		return pUnit != pSourceUnit && pUnit->dwClassId % 3 != 0;
	}

	std::vector<int> get_unit_indices(const UnitRoomScene& scene, D2UnitStrc** ppUnits, int nCount)
	{
		std::vector<int> result;
		for (auto i = 0; i < nCount; ++i)
		{
			result.push_back(scene.IndexOf(ppUnits[i]));
		}
		return result;
	}
}


TEST_SUITE("UnitFindsTests")
{
	const auto working_directory = std::filesystem::current_path();
	const auto dll_base = reinterpret_cast<uintptr_t>(LoadLibraryA((working_directory / "D2Common.dll").string().c_str()));


	TEST_CASE_FIXTURE(NoOpFixture, "D2Common.0x6FDBC680 (#10408)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_AreUnitsInNeighboredRooms, dll_base + 0x0007C680);

		SUBCASE("")
		{
			// Units 0-4 are in rooms 0-4, unit 5 has no room and unit 6 has no path
			std::vector<UnitRoomSceneUnitDesc> units;
			for (auto i = 0; i < find_room_count; ++i)
			{
				const auto& room = find_rooms[i];
				units.push_back(make_unit_at({ i % 2 ? UNIT_MONSTER : UNIT_OBJECT, i, 0, 0 }, i, room.nSubtileX + 1, room.nSubtileY + 1));
			}
			units.push_back(make_unit_at({ UNIT_PLAYER, 0, 0, 0 }, -1, 120, 120));
			units.push_back(make_unit_at({ UNIT_MISSILE, 0, 0, 0 }, 0, 120, 120));
			units.back().bHasPath = FALSE;

			for (auto dest_index = 0; dest_index < static_cast<int>(units.size()); ++dest_index)
			{
				for (auto src_index = 0; src_index < static_cast<int>(units.size()); ++src_index)
				{
					// Input data
					UnitRoomScene moo_scene(find_rooms, units);
					UnitRoomScene original_scene(find_rooms, units);

					// Call both implementations
					const auto moo_result = sut(moo_scene.Unit(dest_index), moo_scene.Unit(src_index));
					const auto original_result = original(original_scene.Unit(dest_index), original_scene.Unit(src_index));

					// Compare return values
					MOO_CHECK_EQ(moo_result, original_result, "Comparing results");

					// Compare potentially modified input data
					check_scenes_eq(moo_scene, original_scene);
				}
			}
		}
	}

	TEST_CASE_FIXTURE(MissilesTxtFixture<MonStatsTxtFixture<NoOpFixture>>, "D2Common.0x6FDBC720 (#11087)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_FindUnitInNeighboredRooms, dll_base + 0x0007C720);

		const auto kinds = make_find_kinds(monstats_txt.get(), monstats_record_count, missiles_txt.get(), missiles_record_count);
		static constexpr auto units_per_room = 6;

		SUBCASE("")
		{
			const auto units = make_room_units(kinds, units_per_room);

			// Every unit in the scene and some combinations which do not exist
			std::vector<std::pair<int, int>> queries;
			for (const auto& unit : units)
			{
				queries.emplace_back(unit.nUnitType, unit.nClassId);
				queries.emplace_back(unit.nUnitType, unit.nClassId + 1000);
			}
			queries.emplace_back(-1, 0);
			queries.emplace_back(UNIT_TYPES_COUNT + 1, 0);

			for (auto room_index = 0; room_index < find_room_count; ++room_index)
			{
				for (const auto& [nUnitType, nClassId] : queries)
				{
					// Input data
					UnitRoomScene moo_scene(find_rooms, units);
					UnitRoomScene original_scene(find_rooms, units);

					const auto setup_data = [](
						UnitRoomScene& scene
					) {
						link_room_units(scene, units_per_room);
					};

					setup_data(moo_scene);
					setup_data(original_scene);

					// Call both implementations
					const auto moo_result = sut(moo_scene.Room(room_index), nUnitType, nClassId);
					const auto original_result = original(original_scene.Room(room_index), nUnitType, nClassId);

					// Compare return values
					CHECK_EQ(moo_scene.IndexOf(moo_result), original_scene.IndexOf(original_result));

					// Compare potentially modified input data
					check_scenes_eq(moo_scene, original_scene);
				}
			}
		}
	}

	TEST_CASE_FIXTURE(MissilesTxtFixture<MonStatsTxtFixture<NoOpFixture>>, "D2Common.0x6FDBC7B0 (#10405)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_GetTestedUnitsFromRoom, dll_base + 0x0007C7B0);

		const auto kinds = make_find_kinds(monstats_txt.get(), monstats_record_count, missiles_txt.get(), missiles_record_count);
		static constexpr auto units_per_room = 7;

		REPEAT_10();

		SUBCASE("")
		{
			const auto units = make_room_units(kinds, units_per_room);

			for (auto room_index = 0; room_index < find_room_count; ++room_index)
			{
				for (const uint32_t mask : { 0u, 0xFFFFFFFFu, random_unsigned_integer(), random_unsigned_integer() })
				{
					// Input data
					UnitRoomScene moo_scene(find_rooms, units);
					UnitRoomScene original_scene(find_rooms, units);
					D2UnitStrc* moo_ppUnits[UNIT_FIND_ARRAY_SIZE]{};
					D2UnitStrc* original_ppUnits[UNIT_FIND_ARRAY_SIZE]{};
					D2UnitFindArgStrc moo_pUnitFindArg{};
					D2UnitFindArgStrc original_pUnitFindArg{};
					UNITFINDTEST pfnUnitTest = test_unit_by_class_mask;

					const auto setup_data = [mask](
						UnitRoomScene& scene,
						D2UnitFindArgStrc& pUnitFindArg
					) {
						link_room_units(scene, units_per_room);
						pUnitFindArg.nFlags = 0x3F;
						pUnitFindArg.unk0x30 = static_cast<int32_t>(mask);
					};

					setup_data(moo_scene, moo_pUnitFindArg);
					setup_data(original_scene, original_pUnitFindArg);

					// Call both implementations
					const auto moo_result = sut(moo_scene.Room(room_index), moo_ppUnits, pfnUnitTest, &moo_pUnitFindArg);
					const auto original_result = original(original_scene.Room(room_index), original_ppUnits, pfnUnitTest, &original_pUnitFindArg);

					// Compare return values
					MOO_CHECK_EQ(moo_result, original_result, "Comparing results");

					// Compare potentially modified input data
					check_scenes_eq(moo_scene, original_scene);
					CHECK_EQ(get_unit_indices(moo_scene, moo_ppUnits, UNIT_FIND_ARRAY_SIZE), get_unit_indices(original_scene, original_ppUnits, UNIT_FIND_ARRAY_SIZE));
					check_unit_find_args_eq(moo_scene, moo_pUnitFindArg, original_scene, original_pUnitFindArg);
				}
			}
		}
	}

	TEST_CASE_FIXTURE(MissilesTxtFixture<MonStatsTxtFixture<NoOpFixture>>, "D2Common.0x6FDBC840 (#11088)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_GetNearestTestedUnit, dll_base + 0x0007C840);

		const auto kinds = make_find_kinds(monstats_txt.get(), monstats_record_count, missiles_txt.get(), missiles_record_count);
		static constexpr auto units_per_room = 6;

		SUBCASE("")
		{
			auto units = make_room_units(kinds, units_per_room);

			// An isolated unit in room 4 and one in room 0 which is not part of its unit list
			units.push_back(make_unit_at(kinds[0], 4, 405, 405));
			units.push_back(make_unit_at(kinds[3], 0, 130, 130));

			const std::vector<std::pair<int, int>> positions = {
				{ 120, 120 }, { 100, 100 }, { 139, 139 }, { 160, 120 }, { 120, 160 }, { 170, 170 }, { 60, 60 }, { 230, 230 }, { 405, 405 }, { -50, 120 },
			};

			for (const auto unit_index : { 0, 5, 11, 17, 23, units_per_room * 4, units_per_room * 4 + 1 })
			{
				// Input data, the scenes are only read, so they are shared by all queries and compared afterwards
				UnitRoomScene moo_scene(find_rooms, units);
				UnitRoomScene original_scene(find_rooms, units);

				const auto setup_data = [](
					UnitRoomScene& scene
				) {
					link_room_units(scene, units_per_room);
					scene.LinkUnitList(4, { units_per_room * 4 });
				};

				setup_data(moo_scene);
				setup_data(original_scene);

				for (const auto& [nX, nY] : positions)
				{
					for (const auto nSize : { 0, 1, 5, 20, 45, 70000 })
					{
						for (const auto pfnUnitTest : { test_nearest_unit_any, test_nearest_unit_by_class })
						{
							// Call both implementations
							const auto moo_result = sut(moo_scene.Unit(unit_index), nX, nY, nSize, pfnUnitTest);
							const auto original_result = original(original_scene.Unit(unit_index), nX, nY, nSize, pfnUnitTest);

							// Compare return values
							CHECK_EQ(moo_scene.IndexOf(moo_result), original_scene.IndexOf(original_result));
						}
					}
				}

				// Compare potentially modified input data
				check_scenes_eq(moo_scene, original_scene);
			}
		}
	}

	TEST_CASE_FIXTURE(NoOpFixture, "D2Common.0x6FDBC990 (#10401)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_InitializeUnitFindData, dll_base + 0x0007C990);
		const auto [moo_free, original_free] = make_function_pair(UNITFINDS_FreeUnitFindData, dll_base + 0x0007CA50);

		SUBCASE("")
		{
			for (auto room_index = -1; room_index < find_room_count; ++room_index)
			{
				for (const auto pfnUnitTest : { (UNITFINDTEST)nullptr, (UNITFINDTEST)test_unit_by_class_mask })
				{
					for (const auto bHasUnitFindArg : { true, false })
					{
						// Input data
						UnitRoomScene moo_scene(find_rooms, {});
						UnitRoomScene original_scene(find_rooms, {});
						D2UnitFindDataStrc moo_pUnitFindData{};
						D2UnitFindDataStrc original_pUnitFindData{};
						D2UnitFindArgStrc moo_pUnitFindArg{};
						D2UnitFindArgStrc original_pUnitFindArg{};
						void* moo_pMemPool = nullptr;
						void* original_pMemPool = nullptr;
						int nX = 117 + room_index;
						int nY = 131 - room_index;
						int nSize = 3 * room_index + 2;

						const auto setup_data = [](
							D2UnitFindDataStrc& pUnitFindData,
							D2UnitFindArgStrc& pUnitFindArg
						) {
							// Every field is expected to be overwritten
							std::memset(&pUnitFindData, 0xCD, sizeof(pUnitFindData));
							pUnitFindArg.nFlags = 0x83;
							pUnitFindArg.nSize = 12;
						};

						setup_data(moo_pUnitFindData, moo_pUnitFindArg);
						setup_data(original_pUnitFindData, original_pUnitFindArg);

						// Call both implementations
						sut(moo_pMemPool, &moo_pUnitFindData, moo_scene.Room(room_index), nX, nY, nSize, pfnUnitTest, bHasUnitFindArg ? &moo_pUnitFindArg : nullptr);
						original(original_pMemPool, &original_pUnitFindData, original_scene.Room(room_index), nX, nY, nSize, pfnUnitTest, bHasUnitFindArg ? &original_pUnitFindArg : nullptr);

						// Compare potentially modified input data
						check_unit_find_data_eq(moo_scene, moo_pUnitFindData, original_scene, original_pUnitFindData);
						CHECK_EQ(moo_pUnitFindData.pMemPool, original_pUnitFindData.pMemPool);
						CHECK_EQ(moo_pUnitFindData.pUnitFindArg == &moo_pUnitFindArg, original_pUnitFindData.pUnitFindArg == &original_pUnitFindArg);
						check_scenes_eq(moo_scene, original_scene);

						// The units array is allocated and cleared
						REQUIRE_NE(moo_pUnitFindData.pUnitsArray, nullptr);
						REQUIRE_NE(original_pUnitFindData.pUnitsArray, nullptr);
						CHECK_EQ(get_unit_indices(moo_scene, moo_pUnitFindData.pUnitsArray, UNIT_FIND_ARRAY_SIZE), get_unit_indices(original_scene, original_pUnitFindData.pUnitsArray, UNIT_FIND_ARRAY_SIZE));
						CHECK_EQ(get_unit_indices(moo_scene, moo_pUnitFindData.pUnitsArray, UNIT_FIND_ARRAY_SIZE), std::vector<int>(UNIT_FIND_ARRAY_SIZE, -1));

						// Free allocated memory
						moo_free(&moo_pUnitFindData);
						original_free(&original_pUnitFindData);
					}
				}
			}
		}
	}

	// NOTE: Still needs to be confirmed manually since freeing memory has no observable behaviour besides the pointer being reset
	TEST_CASE_FIXTURE(NoOpFixture, "D2Common.0x6FDBCA50 (#10402)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_FreeUnitFindData, dll_base + 0x0007CA50);
		const auto [moo_init, original_init] = make_function_pair(UNITFINDS_InitializeUnitFindData, dll_base + 0x0007C990);

		SUBCASE("")
		{
			for (const auto bAllocated : { true, false })
			{
				// Input data
				UnitRoomScene moo_scene(find_rooms, {});
				UnitRoomScene original_scene(find_rooms, {});
				D2UnitFindDataStrc moo_pUnitFindData{};
				D2UnitFindDataStrc original_pUnitFindData{};

				const auto setup_data = [bAllocated](
					UnitRoomScene& scene,
					D2UnitFindDataStrc& pUnitFindData,
					const auto& init
				) {
					if (bAllocated)
					{
						init(nullptr, &pUnitFindData, scene.Room(2), 120, 150, 8, nullptr, nullptr);
					}
					pUnitFindData.nIndex = 4;
					pUnitFindData.nFlags = 0x2000;
				};

				setup_data(moo_scene, moo_pUnitFindData, moo_init);
				setup_data(original_scene, original_pUnitFindData, original_init);

				// Call both implementations
				sut(&moo_pUnitFindData);
				original(&original_pUnitFindData);

				// Compare potentially modified input data
				check_unit_find_data_eq(moo_scene, moo_pUnitFindData, original_scene, original_pUnitFindData);
				CHECK_EQ(moo_pUnitFindData.pUnitsArray, nullptr);
				CHECK_EQ(original_pUnitFindData.pUnitsArray, nullptr);
			}
		}

		SUBCASE("nullptr")
		{
			// Call both implementations
			sut(nullptr);
			original(nullptr);
		}
	}

	TEST_CASE_FIXTURE(MissilesTxtFixture<MonStatsTxtFixture<NoOpFixture>>, "D2Common.0x6FDBCA80 (#10403)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_FindAllMatchingUnitsInNeighboredRooms, dll_base + 0x0007CA80);
		const auto [moo_init, original_init] = make_function_pair(UNITFINDS_InitializeUnitFindData, dll_base + 0x0007C990);
		const auto [moo_free, original_free] = make_function_pair(UNITFINDS_FreeUnitFindData, dll_base + 0x0007CA50);

		const auto kinds = make_find_kinds(monstats_txt.get(), monstats_record_count, missiles_txt.get(), missiles_record_count);
		static constexpr auto units_per_room = 9;

		// Positions inside a single room, at the borders of rooms and outside of all rooms
		const std::vector<std::tuple<int, int, int>> areas = {
			{ 120, 120, 5 }, { 120, 120, 19 }, { 120, 120, 20 }, { 105, 130, 4 }, { 104, 130, 4 }, { 160, 120, 10 },
			{ 150, 150, 3 }, { 120, 170, 30 }, { 140, 140, 100 }, { 400, 120, 10 }, { 60, 60, 10 }, { 410, 410, 2 },
		};

		// The unit find argument flags which can be tested without collision data
		const int32_t arg_flags[] = { 0x1, 0x2, 0x6, 0x8, 0x10, 0x20, 0x3F, 0x103B, 0x43F, 0x4BF, 0x13F, 0x83F, 0x7F };

		SUBCASE("")
		{
			const auto units = make_room_units(kinds, units_per_room);

			for (auto room_index = -1; room_index < find_room_count; ++room_index)
			{
				for (const auto& [nX, nY, nSize] : areas)
				{
					for (const auto nFlags : { 0, 0x2000 })
					{
							for (const auto bCustomTest : { false, true })
							{
								// The scenes are only read, so they are shared by all argument flags and compared afterwards
								UnitRoomScene moo_scene(find_rooms, units);
								UnitRoomScene original_scene(find_rooms, units);
								link_room_units(moo_scene, units_per_room);
								link_room_units(original_scene, units_per_room);

								for (const auto nArgFlags : arg_flags)
								{
									// The flags of the argument only matter for UNITFINDS_TestUnit
									if (bCustomTest && nArgFlags != arg_flags[0])
									{
										continue;
									}

									// Input data
									D2UnitFindDataStrc moo_pUnitFindData{};
									D2UnitFindDataStrc original_pUnitFindData{};
									D2UnitFindArgStrc moo_pUnitFindArg{};
									D2UnitFindArgStrc original_pUnitFindArg{};

									const auto setup_data = [&, nX = nX, nY = nY, nSize = nSize](
										UnitRoomScene& scene,
										D2UnitFindDataStrc& pUnitFindData,
										D2UnitFindArgStrc& pUnitFindArg,
										const auto& init
									) {
										pUnitFindArg.nFlags = nArgFlags;
										pUnitFindArg.pUnit = scene.Unit(units_per_room);
										pUnitFindArg.nX = nX;
										pUnitFindArg.nY = nY;
										pUnitFindArg.nSize = nSize;
										pUnitFindArg.nMaxArrayEntries = 9;
										pUnitFindArg.pfnUnitTest = test_unit_by_class_mask;
										pUnitFindArg.unk0x30 = 0x5A5A5A5A;

										init(nullptr, &pUnitFindData, scene.Room(room_index), nX, nY, nSize, bCustomTest ? test_unit_by_class_mask : nullptr, &pUnitFindArg);
										pUnitFindData.nFlags = nFlags;
									};

									setup_data(moo_scene, moo_pUnitFindData, moo_pUnitFindArg, moo_init);
									setup_data(original_scene, original_pUnitFindData, original_pUnitFindArg, original_init);

									// Call both implementations
									sut(&moo_pUnitFindData);
									original(&original_pUnitFindData);

									// Compare potentially modified input data
									check_unit_find_data_eq(moo_scene, moo_pUnitFindData, original_scene, original_pUnitFindData);
									CHECK_EQ(get_unit_indices(moo_scene, moo_pUnitFindData.pUnitsArray, moo_pUnitFindData.nIndex), get_unit_indices(original_scene, original_pUnitFindData.pUnitsArray, original_pUnitFindData.nIndex));
									check_unit_find_args_eq(moo_scene, moo_pUnitFindArg, original_scene, original_pUnitFindArg);

									// Free allocated memory
									moo_free(&moo_pUnitFindData);
									original_free(&original_pUnitFindData);
								}

								check_scenes_eq(moo_scene, original_scene);
							}
					}
				}
			}
		}

		SUBCASE("nullptr pUnitFindArg")
		{
			const auto units = make_room_units(kinds, units_per_room);

			for (auto room_index = -1; room_index < find_room_count; ++room_index)
			{
				// Input data
				UnitRoomScene moo_scene(find_rooms, units);
				UnitRoomScene original_scene(find_rooms, units);
				D2UnitFindDataStrc moo_pUnitFindData{};
				D2UnitFindDataStrc original_pUnitFindData{};

				const auto setup_data = [room_index](
					UnitRoomScene& scene,
					D2UnitFindDataStrc& pUnitFindData,
					const auto& init
				) {
					link_room_units(scene, units_per_room);
					init(nullptr, &pUnitFindData, scene.Room(room_index), 140, 140, 50, nullptr, nullptr);
					pUnitFindData.nIndex = 3;
				};

				setup_data(moo_scene, moo_pUnitFindData, moo_init);
				setup_data(original_scene, original_pUnitFindData, original_init);

				// Call both implementations
				sut(&moo_pUnitFindData);
				original(&original_pUnitFindData);

				// Compare potentially modified input data
				check_unit_find_data_eq(moo_scene, moo_pUnitFindData, original_scene, original_pUnitFindData);
				check_scenes_eq(moo_scene, original_scene);

				// Free allocated memory
				moo_free(&moo_pUnitFindData);
				original_free(&original_pUnitFindData);
			}
		}
	}

	TEST_CASE_FIXTURE(MissilesTxtFixture<MonStatsTxtFixture<NoOpFixture>>, "D2Common.0x6FDBCCA0 (#10404)")
	{
		// Set up function pointers
		const auto [sut, original] = make_function_pair(UNITFINDS_TestUnit, dll_base + 0x0007CCA0);

		const auto kinds = make_find_kinds(monstats_txt.get(), monstats_record_count, missiles_txt.get(), missiles_record_count);

		// Units of every kind in a regular room, in town and without room, at distances below, at and above the search radius of 10 (the units are never modified, so they are compared once per flag set)
		std::vector<UnitRoomSceneUnitDesc> units;
		for (const auto& kind : kinds)
		{
			for (const auto room_index : { 0, 1, -1 })
			{
				for (const auto& [nOffsetX, nOffsetY] : { std::pair{ 0, 0 }, std::pair{ 6, -8 }, std::pair{ 0, -11 } })
				{
					for (const uint32_t dwFlags : { 0u, (uint32_t)UNITFLAG_CANBEATTACKED, (uint32_t)UNITFLAG_ISVALIDTARGET, kind.dwFlags | UNITFLAG_ISINIT })
					{
						auto unit_kind = kind;
						unit_kind.dwFlags = dwFlags;
						units.push_back(make_unit_at(unit_kind, room_index, 125 + nOffsetX, 115 + nOffsetY));
					}
				}
			}
		}

		// UNITFINDS_TestUnit only reads the units, so the scenes are shared by all calls of a subcase and compared at its end
		const auto test_flags = [&](UnitRoomScene& moo_scene, UnitRoomScene& original_scene, int32_t nArgFlags, int32_t nMaxArrayEntries)
		{
			// Input data
			D2UnitFindArgStrc moo_pUnitFindArg{};
			D2UnitFindArgStrc original_pUnitFindArg{};

			const auto setup_data = [&](
				UnitRoomScene& scene,
				D2UnitFindArgStrc& pUnitFindArg
			) {
				pUnitFindArg.nFlags = nArgFlags;
				pUnitFindArg.unk0x04 = 0x11;
				pUnitFindArg.pUnit = scene.Unit(0);
				pUnitFindArg.nX = 125;
				pUnitFindArg.nY = 115;
				pUnitFindArg.nSize = 10;
				pUnitFindArg.nMaxArrayEntries = nMaxArrayEntries;
				pUnitFindArg.pfnUnitTest = test_unit_by_class_mask;
				pUnitFindArg.unk0x30 = 0x5A5A5A5A;
			};

			setup_data(moo_scene, moo_pUnitFindArg);
			setup_data(original_scene, original_pUnitFindArg);

			for (auto unit_index = 0; unit_index < static_cast<int>(units.size()); ++unit_index)
			{
				// Collision checks need collision data, so they are only tested for units without room
				const auto bSkipCollision = (nArgFlags & 0x200) && units[unit_index].nRoom >= 0;
				if (bSkipCollision)
				{
					continue;
				}

				// Call both implementations
				const auto moo_result = sut(moo_scene.Unit(unit_index), &moo_pUnitFindArg);
				const auto original_result = original(original_scene.Unit(unit_index), &original_pUnitFindArg);

				// Compare return values
				MOO_CHECK_EQ(moo_result, original_result, "Comparing results");

				// Compare potentially modified input data
				check_unit_find_args_eq(moo_scene, moo_pUnitFindArg, original_scene, original_pUnitFindArg);
			}
		};

		// Input data
		UnitRoomScene moo_scene(find_rooms, units);
		UnitRoomScene original_scene(find_rooms, units);

		SUBCASE("Single flags")
		{
			for (auto i = 0; i < 14; ++i)
			{
				test_flags(moo_scene, original_scene, 1 << i, 1000000);
				test_flags(moo_scene, original_scene, (1 << i) | 0x3B, 1000000);
			}
		}

		SUBCASE("Flag combinations")
		{
			for (const auto nArgFlags : { 0x3F, 0x103F, 0x7F, 0xBF, 0x43F, 0x4BF, 0x13F, 0x83F, 0x23F, 0x1FBF, 0x0 })
			{
				test_flags(moo_scene, original_scene, nArgFlags, 1000000);
				test_flags(moo_scene, original_scene, nArgFlags | 0x40, 25);
				test_flags(moo_scene, original_scene, nArgFlags | 0x40, 0);
			}
		}

		SUBCASE("Random flags")
		{
			REPEAT_10();

			test_flags(moo_scene, original_scene, static_cast<int32_t>(random_unsigned_integer()), static_cast<int32_t>(random_unsigned_integer(0, 200)));
		}

		SUBCASE("nullptr pUnitFindArg")
		{
			for (auto unit_index = 0; unit_index < static_cast<int>(units.size()); unit_index += 7)
			{
				// Call both implementations
				const auto moo_result = sut(moo_scene.Unit(unit_index), nullptr);
				const auto original_result = original(original_scene.Unit(unit_index), nullptr);

				// Compare return values
				MOO_CHECK_EQ(moo_result, original_result, "Comparing results");
			}
		}

		// Compare potentially modified input data
		check_scenes_eq(moo_scene, original_scene);
	}
}
#endif
