#pragma once
#include "qt_bridge.hpp"
namespace muse {
struct BridgeTestAccess {
    static void holdBackground(Bridge& bridge,std::shared_future<void> gate){bridge.work([gate]{gate.wait();return QtResult{};},true);}
    static void rebuildCatalog(Bridge& bridge){bridge.rebuildProcesses();}
    static void publishPartialCatalog(Bridge& bridge,QVariantList rows){bridge.applyCatalogResult(std::move(rows),false);}
    static void markConnected(Bridge& bridge){bridge.connected_=true;bridge.loaded_=true;}
};
}
