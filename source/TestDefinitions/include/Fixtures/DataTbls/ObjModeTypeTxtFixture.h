#pragma once


#include <cstdarg>
#include <cstring>
#include <memory>
#include <utility>

#include <Windows.h>

#include <D2DataTbls.h>


// ObjType.txt and ObjMode.txt are not shipped as compiled .bin files, so the records are built from the 1.10f ObjMode tokens and a representative set of ObjType tokens
template<class Fixture>
struct ObjModeTypeTxtFixture : Fixture
{
	std::unique_ptr<D2ObjModeTypeTxt[]> objmodetype_txt;
	int objtype_record_count;
	int objmode_record_count;

	ObjModeTypeTxtFixture()
	{
		const auto working_directory = std::filesystem::current_path();
		const auto d2common_base = reinterpret_cast<uintptr_t>(LoadLibraryA((working_directory / "D2Common.dll").string().c_str()));

		const auto original_sgptDataTables = reinterpret_cast<D2DataTablesStrc**>(d2common_base + 0x00096A20);

		constexpr const char* objtype[][2] = {
			{ "Barrel", "B1" }, { "Chest", "C1" }, { "Waypoint", "WP" }, { "Torch", "T1" }, { "Door", "D1" },
			{ "Shrine", "SH" }, { "Stash", "ST" }, { "Well", "W1" }, { "Portal", "PT" }, { "Bonfire", "BF" },
			{ "Corpse", "C" }, { "Rock", "RK1" },
		};

		constexpr const char* objmode[][2] = {
			{ "Neutral", "NU" }, { "Operating", "OP" }, { "Opened", "ON" }, { "Special1", "S1" },
			{ "Special2", "S2" }, { "Special3", "S3" }, { "Special4", "S4" }, { "Special5", "S5" },
		};

		const auto type_count = static_cast<int>(std::size(objtype));
		const auto mode_count = static_cast<int>(std::size(objmode));

		auto records = std::make_unique<D2ObjModeTypeTxt[]>(type_count + mode_count);
		std::memset(records.get(), 0, sizeof(D2ObjModeTypeTxt) * (type_count + mode_count));

		const auto fill_record = [](D2ObjModeTypeTxt& record, const char* name, const char* token)
		{
			strcpy_s(record.szName, sizeof(record.szName), name);
			std::memset(record.szToken, ' ', 4);
			std::memcpy(record.szToken, token, std::strlen(token));
		};

		for (auto i = 0; i < type_count; ++i)
		{
			fill_record(records[i], objtype[i][0], objtype[i][1]);
		}

		for (auto i = 0; i < mode_count; ++i)
		{
			fill_record(records[type_count + i], objmode[i][0], objmode[i][1]);
		}

		sgptDataTables->pObjModeDataTables.nObjModeTypeTxtRecordCount = type_count + mode_count;
		sgptDataTables->pObjModeDataTables.pObjModeTypeTxt = records.get();
		sgptDataTables->pObjModeDataTables.pObjType = records.get();
		sgptDataTables->pObjModeDataTables.pObjMode = &records[type_count];

		*original_sgptDataTables = sgptDataTables;

		const auto original_record_count = reinterpret_cast<int*>(d2common_base + 0x000A9608 + 0x000010C4);
		*original_record_count = type_count + mode_count;

		const auto original_objmodetype = reinterpret_cast<D2ObjModeTypeTxt**>(d2common_base + 0x000A9608 + 0x000010C8);
		*original_objmodetype = records.get();

		const auto original_objtype = reinterpret_cast<D2ObjModeTypeTxt**>(d2common_base + 0x000A9608 + 0x000010CC);
		*original_objtype = records.get();

		const auto original_objmode = reinterpret_cast<D2ObjModeTypeTxt**>(d2common_base + 0x000A9608 + 0x000010D0);
		*original_objmode = &records[type_count];

		objmodetype_txt = std::move(records);
		objtype_record_count = type_count;
		objmode_record_count = mode_count;
	};
};
