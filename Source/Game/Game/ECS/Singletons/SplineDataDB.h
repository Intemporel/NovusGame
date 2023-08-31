#pragma once
#include <Base/Types.h>

#include <FileFormat/Novus/ClientDB/ClientDB.h>
#include <FileFormat/Novus/ClientDB/Definitions.h>

#include <robinhood/robinhood.h>

namespace ECS::Singletons
{
    struct SplineDataDB
    {
    public:
        SplineDataDB() {};

        DB::Client::ClientDB<DB::Client::Definitions::SplineData> entries;

        robin_hood::unordered_map<u32, std::string> splineEntryIDToPath;
    };
}