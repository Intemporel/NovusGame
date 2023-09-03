#pragma once
#include <Base/Types.h>

#include <FileFormat/Novus/ClientDB/ClientDB.h>
#include <FileFormat/Novus/ClientDB/Definitions.h>

#include <robinhood/robinhood.h>

namespace ECS::Singletons
{
    struct CinematicDB
    {
    public:
        CinematicDB() {};

        DB::Client::ClientDB<DB::Client::Definitions::Cinematic> entries;

        std::vector<std::string> cinematicNames;
    };
}