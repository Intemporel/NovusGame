#include "Game/Loaders/LoaderSystem.h"
#include "Game/Util/ServiceLocator.h"
#include "Game/ECS/Singletons/MapDB.h"
#include "Game/ECS/Singletons/CinematicDB.h"
#include "Game/ECS/Singletons/SplineDataDB.h"
#include "Game/Application/EnttRegistries.h"

#include <Base/Container/ConcurrentQueue.h>
#include <Base/Memory/FileReader.h>
#include <Base/Util/DebugHandler.h>

#include <entt/entt.hpp>

#include <execution>
#include <filesystem>
#include <limits>
namespace fs = std::filesystem;

struct ClientDBPair
{
    u32 hash;
    std::string path;
};

class ClientDBLoader : Loader
{
public:
    ClientDBLoader() : Loader("ClientDBLoader", 9999) { }

    bool Init()
    {
        EnttRegistries* registries = ServiceLocator::GetEnttRegistries();
        
        entt::registry* registry = registries->gameRegistry;
        entt::registry::context& ctx = registry->ctx();

        SetupSingletons(ctx);

        static const fs::path fileExtension = ".cdb";
        fs::path relativeParentPath = "Data/ClientDB";
        fs::path absolutePath = std::filesystem::absolute(relativeParentPath).make_preferred();
        fs::create_directories(absolutePath);
        
        moodycamel::ConcurrentQueue<ClientDBPair> clientDBPairs;

        std::vector<std::filesystem::path> paths;
        std::filesystem::recursive_directory_iterator dirpos{ absolutePath };
        std::copy(begin(dirpos), end(dirpos), std::back_inserter(paths));

        std::for_each(std::execution::par, std::begin(paths), std::end(paths), [&clientDBPairs](const std::filesystem::path& path)
        {
            if (!path.has_extension() || path.extension().compare(fileExtension) != 0)
                return;

            std::string fileName = path.filename().string();

            ClientDBPair clientDBPair;
            clientDBPair.path = path.string();
            clientDBPair.hash = StringUtils::fnv1a_32(fileName.c_str(), fileName.length());

            clientDBPairs.enqueue(clientDBPair);
        });

        size_t numClientDBs = 0;

        std::shared_ptr<Bytebuffer> buffer = Bytebuffer::Borrow<8388608>();

        ClientDBPair clientDBPair;
        while (clientDBPairs.try_dequeue(clientDBPair))
        {
            // Find appropriate handler for the given hash
            auto itr = clientDBEntries.find(clientDBPair.hash);
            if (itr != clientDBEntries.end())
            {
                buffer->Reset();

                FileReader reader(clientDBPair.path);
                if (reader.Open())
                {
                    reader.Read(buffer.get(), reader.Length());

                    if (itr->second(ctx, buffer, clientDBPair))
                    {
                        numClientDBs++;
                        continue;
                    }
                }


                DebugHandler::PrintError("ClientDBLoader : Failed to load '{0}'", clientDBPair.path);
            }
        }

        DebugHandler::Print("Loaded {0} Client Database Files", numClientDBs);

        return true;
    }

    void SetupSingletons(entt::registry::context& registryCtx)
    {
        registryCtx.emplace<ECS::Singletons::MapDB>();
        registryCtx.emplace<ECS::Singletons::CinematicDB>();
        registryCtx.emplace<ECS::Singletons::SplineDataDB>();
    }

    bool LoadMapDB(entt::registry::context& registryCtx, std::shared_ptr<Bytebuffer>& buffer, const ClientDBPair& pair)
    {
        auto& mapDB = registryCtx.at<ECS::Singletons::MapDB>();

        // Clear, in case we already filled mapDB
        mapDB.entries.data.clear();
        mapDB.entries.stringTable.Clear();
     
        if (!mapDB.entries.Read(buffer))
        {
            return false;
        }

        StringTable& stringTable = mapDB.entries.stringTable;

        u32 numRecords = static_cast<u32>(mapDB.entries.data.size());
        mapDB.mapNames.reserve(numRecords);
        mapDB.mapInternalNames.reserve(numRecords);
        mapDB.mapNameHashToEntryID.reserve(numRecords);

        for (u32 i = 0; i < numRecords; i++)
        {
            const DB::Client::Definitions::Map& map = mapDB.entries.data[i];
            if (map.name == std::numeric_limits<u32>().max())
            {
                continue;
            }

            const std::string& mapName = stringTable.GetString(map.name);
            u32 mapNameHash = StringUtils::fnv1a_32(mapName.c_str(), mapName.length());

            mapDB.mapNames.push_back(mapName);
            mapDB.mapInternalNames.push_back(stringTable.GetString(map.internalName));
            mapDB.mapNameHashToEntryID[mapNameHash] = i;
        }

        return true;
    }

    bool LoadCinematicDB(entt::registry::context& registryCtx, std::shared_ptr<Bytebuffer>& buffer, const ClientDBPair& pair)
    {
        auto& cinematicDB = registryCtx.at<ECS::Singletons::CinematicDB>();

        cinematicDB.entries.data.clear();
        cinematicDB.entries.stringTable.Clear();

        bool isRead = cinematicDB.entries.Read(buffer);
        return isRead;
    }

    bool LoadSplineDataDB(entt::registry::context& registryCtx, std::shared_ptr<Bytebuffer>& buffer, const ClientDBPair& pair)
    {
        auto& splineDataDB = registryCtx.at<ECS::Singletons::SplineDataDB>();

        splineDataDB.entries.data.clear();
        splineDataDB.entries.stringTable.Clear();

        if (!splineDataDB.entries.Read(buffer))
        {
            return false;
        }

        u32 numRecords = static_cast<u32>(splineDataDB.entries.data.size());
        splineDataDB.splineEntryIDToPath.reserve(numRecords);

        fs::path splineExtension = ".spline";
        fs::path rootSplinePath = fs::path("Data/Spline/");
        fs::path absoluteSplinePath = fs::absolute(rootSplinePath);

        robin_hood::unordered_map<u32, std::string> splineHashToName;
        for (const fs::path& path : fs::recursive_directory_iterator(absoluteSplinePath))
        {
            if (!path.has_extension() || path.extension().compare(splineExtension) != 0)
                continue;

            fs::path relativePath = rootSplinePath / fs::relative(path, rootSplinePath);
            std::string splinePath = relativePath.string();
            std::replace(splinePath.begin(), splinePath.end(), L'\\', L'/');

            u32 splineHash = StringUtils::fnv1a_32(splinePath.c_str(), splinePath.length());
            splineHashToName[splineHash] = splinePath;
        }

        for (u32 i = 0; i < numRecords; i++)
        {
            u32 id = splineDataDB.entries.data[i].id;
            u32 hash = splineDataDB.entries.data[i].path;

            if (splineHashToName[hash].empty())
                continue;

            splineDataDB.splineEntryIDToPath[id] = splineHashToName[hash];
        }

        return true;
    }

    robin_hood::unordered_map<u32, std::function<bool(entt::registry::context&, std::shared_ptr<Bytebuffer>&, const ClientDBPair&)>> clientDBEntries =
    {
        { "Map.cdb"_h, std::bind(&ClientDBLoader::LoadMapDB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
        { "Cinematic.cdb"_h, std::bind(&ClientDBLoader::LoadCinematicDB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
        { "SplineData.cdb"_h, std::bind(&ClientDBLoader::LoadSplineDataDB, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) },
    };
};

ClientDBLoader clientDBLoader;