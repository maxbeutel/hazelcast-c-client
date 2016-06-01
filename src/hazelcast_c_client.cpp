//
// export hazelcast methods
//
// http://www.oracle.com/technetwork/articles/servers-storage-dev/mixingcandcpluspluscode-305840.html#cpp_from_c
// https://isocpp.org/wiki/faq/mixing-c-and-cpp
// https://github.com/emnl/leveldb-c-example/blob/master/leveldb_example.c
// https://github.com/google/leveldb/blob/7306ef856a91e462a73ff1832c1fa8771008ba36/db/c.cc
//

#include <string.h>

#include <hazelcast_c_client.h>
#include <hazelcast/client/HazelcastAll.h>

using namespace std;
using namespace hazelcast::client;
using namespace hazelcast::client::proxy;
using namespace hazelcast::client::spi;
using namespace hazelcast::client::serialization::pimpl;

class RawDataIMapImpl : protected IMapImpl
{
public:
    RawDataIMapImpl(const std::string &instanceName, spi::ClientContext *context)
            : IMapImpl(instanceName, context) {};

    using IMapImpl::set;
    using IMapImpl::getData;
    using IMapImpl::containsKey;
    using IMapImpl::deleteEntry;
    using IMapImpl::size;
};

/* Internal */
static void saveMessageInErrPtr(char **errPtr, const char *message)
{
    assert(errPtr != NULL);

    if (message == NULL) {
        *errPtr = strdup("(HAZELCAST ERROR occurred, but message was empty)");
    } else {
        if (*errPtr == NULL) {
            *errPtr = strdup(message);
        } else {
            free(*errPtr);
            *errPtr = strdup(message);
        }
    }
}

static void saveUnknownErrorOccurredMessageInErrPtr(char **errPtr)
{
    assert(errPtr != NULL);

    saveMessageInErrPtr(errPtr, "Unknown failure occurred. Possible memory corruption.");
}

/* Exported types */
extern "C" struct Hazelcast_ClientConfig_t {
    ClientConfig *config;
};

extern "C" struct Hazelcast_Client_t {
    HazelcastClient *client;
    ClientContext *context;
};

extern "C" struct Hazelcast_Data_t {
    Data data;
};

/* Serialization functions */
// string
extern "C" Hazelcast_Data_t *Hazelcast_Serialization_stringToData(
    const Hazelcast_Client_t *client,
    const char *stringValue,
    size_t len
)
{
    assert(client != NULL);
    assert(client->context != NULL);

    assert(stringValue != NULL);

    assert(len > 0);

    // http://stackoverflow.com/questions/347949/how-to-convert-a-stdstring-to-const-char-or-char
    // @TODO see exception safety

    const std::string stdstr(stringValue, len);
    Data hazelcastData = client->context->getSerializationService().toData(&stdstr);

    Hazelcast_Data_t *data = new Hazelcast_Data_t();
    data->data = hazelcastData;

    return data;
}

extern "C" char *Hazelcast_Serialization_dataToString(
    const Hazelcast_Client_t *client,
    const Hazelcast_Data_t *data
)
{
    assert(client != NULL);
    assert(client->context != NULL);

    assert(data != NULL);

    // @TODO is it actually OK for this to be NULL, e. g. if a key is not found?
    std::auto_ptr<std::string> stringPtr = client->context->getSerializationService().toObject<std::string>(data->data);
    std::string *stringValue = stringPtr.get();

    //assert(stringValue != NULL);

    if (stringValue != NULL) {
        return strdup(stringValue->c_str());
    }

    return NULL;
}

// int
extern "C" Hazelcast_Data_t *Hazelcast_Serialization_intToData(
    const Hazelcast_Client_t *client,
    int intValue
)
{
    assert(client != NULL);
    assert(client->context != NULL);

    Data hazelcastData = client->context->getSerializationService().toData(&intValue);

    Hazelcast_Data_t *data = new Hazelcast_Data_t();
    data->data = hazelcastData;

    return data;
}

extern "C" int Hazelcast_Serialization_dataToInt(
    const Hazelcast_Client_t *client,
    const Hazelcast_Data_t *data
)
{
    assert(client != NULL);
    assert(client->context != NULL);

    assert(data != NULL);
    // @TODO assert that data->data is not NULL?

    std::auto_ptr<int> intPtr = client->context->getSerializationService().toObject<int>(data->data);
    int *intValue = intPtr.get();

    if (intValue != NULL) {
        return *intValue;
    }

    return 0;
}

extern "C" void Hazelcast_Data_destroy(Hazelcast_Data_t *data)
{
    if (data != NULL) {
        delete data;
    }
}

/* Configuration */
extern "C" Hazelcast_ClientConfig_t* Hazelcast_ClientConfig_create()
{
     Hazelcast_ClientConfig_t *clientConfig = new Hazelcast_ClientConfig_t;
     clientConfig->config = new ClientConfig();

     return clientConfig;
}

extern "C" void Hazelcast_ClientConfig_destroy(Hazelcast_ClientConfig_t *clientConfig)
{
    if (clientConfig != NULL && clientConfig->config != NULL) {
        delete clientConfig->config;
    }

    if (clientConfig != NULL) {
        delete clientConfig;
    }
}

extern "C" void Hazelcast_ClientConfig_add_address(
    const Hazelcast_ClientConfig_t *clientConfig,
    const char *networkAddress,
    int port
)
{
    assert(clientConfig != NULL);
    assert(clientConfig->config != NULL);

    assert(networkAddress != NULL);
    assert(port >= 0 && port <= 65535);

    Address address(networkAddress, port);
    clientConfig->config->addAddress(address);
}

/* Client */
extern "C" Hazelcast_Client_t *Hazelcast_Client_create(
    Hazelcast_ClientConfig_t *clientConfig,
    char **errPtr
)
{
    assert(clientConfig != NULL);
    assert(clientConfig->config != NULL);

    try {
        Hazelcast_Client_t *client = new Hazelcast_Client_t;
        client->client = new HazelcastClient(*(clientConfig->config));
        client->context = new ClientContext(*client->client);

        return client;
    } catch(const std::runtime_error& re) {
        saveMessageInErrPtr(errPtr, re.what());
    } catch(const std::exception& ex) {
        saveMessageInErrPtr(errPtr, ex.what());
    } catch(...) {
        saveUnknownErrorOccurredMessageInErrPtr(errPtr);
    }

    return NULL;
}

extern "C" void Hazelcast_Client_destroy(Hazelcast_Client_t *client)
{
    if (client != NULL && client->client != NULL) {
        delete client->client;
    }

    if (client != NULL && client->context != NULL) {
        delete client->context;
    }

    if (client != NULL) {
        delete client;
    }
}

/* Map operations */
extern void Hazelcast_Map_set(
    const Hazelcast_Client_t *hazelcastClient,
    const char *mapName,
    const Hazelcast_Data_t *key,
    const Hazelcast_Data_t *value,
    long ttl,
    char** errPtr
)
{
    assert(hazelcastClient != NULL);
    assert(hazelcastClient->client != NULL);
    assert(hazelcastClient->context != NULL);

    assert(mapName != NULL);

    assert(key != NULL);
//    assert(key->data != NULL);

    assert(value != NULL);
//  assert(value->data != NULL);

    try {
        RawDataIMapImpl mapImpl(mapName, hazelcastClient->context);
        mapImpl.set(key->data, value->data, ttl);
    } catch(const std::runtime_error& re) {
        saveMessageInErrPtr(errPtr, re.what());
    } catch(const std::exception& ex) {
        saveMessageInErrPtr(errPtr, ex.what());
    } catch(...) {
        saveUnknownErrorOccurredMessageInErrPtr(errPtr);
    }
}

extern Hazelcast_Data_t *Hazelcast_Map_get(
    const Hazelcast_Client_t *hazelcastClient,
    const char *mapName,
    const Hazelcast_Data_t *key,
    char **errPtr
)
{
    assert(hazelcastClient != NULL);
    assert(hazelcastClient->client != NULL);
    assert(hazelcastClient->context != NULL);

    assert(mapName != NULL);

    assert(key != NULL);

    try {
        RawDataIMapImpl mapImpl(mapName, hazelcastClient->context);
        std::auto_ptr<Data> dataPtr = mapImpl.getData(key->data);
        Data *hazelcastData = dataPtr.get();

        if (hazelcastData != NULL) {
            Hazelcast_Data_t *data = new Hazelcast_Data_t();
            data->data = *hazelcastData;

            return data;
        }

        return NULL;
    } catch(const std::runtime_error& re) {
        saveMessageInErrPtr(errPtr, re.what());
    } catch(const std::exception& ex) {
        saveMessageInErrPtr(errPtr, ex.what());
    } catch(...) {
        saveUnknownErrorOccurredMessageInErrPtr(errPtr);
    }

    return NULL;
}

extern int Hazelcast_Map_containsKey(
    const Hazelcast_Client_t *hazelcastClient,
    const char *mapName,
    const Hazelcast_Data_t *key,
    char **errPtr
)
{
    assert(hazelcastClient != NULL);
    assert(hazelcastClient->client != NULL);
    assert(hazelcastClient->context != NULL);

    assert(mapName != NULL);

    assert(key != NULL);
//    assert(key->data != NULL);

    try {
        RawDataIMapImpl mapImpl(mapName, hazelcastClient->context);

        if (mapImpl.containsKey(key->data)) {
            return 1;
        } else {
            return 0;
        }
    } catch(const std::runtime_error& re) {
        saveMessageInErrPtr(errPtr, re.what());
    } catch(const std::exception& ex) {
        saveMessageInErrPtr(errPtr, ex.what());
    } catch(...) {
        saveUnknownErrorOccurredMessageInErrPtr(errPtr);
    }

    return -1;
}

extern void Hazelcast_Map_delete(
    const Hazelcast_Client_t *hazelcastClient,
    const char *mapName,
    const Hazelcast_Data_t *key,
    char **errPtr
)
{
    assert(hazelcastClient != NULL);
    assert(hazelcastClient->client != NULL);
    assert(hazelcastClient->context != NULL);

    assert(mapName != NULL);

    assert(key != NULL);
//    assert(key->data != NULL);

    try {
        RawDataIMapImpl mapImpl(mapName, hazelcastClient->context);
        mapImpl.deleteEntry(key->data);
    } catch(const std::runtime_error& re) {
        saveMessageInErrPtr(errPtr, re.what());
    } catch(const std::exception& ex) {
        saveMessageInErrPtr(errPtr, ex.what());
    } catch(...) {
        saveUnknownErrorOccurredMessageInErrPtr(errPtr);
    }
}

extern int Hazelcast_Map_size(
    const Hazelcast_Client_t *hazelcastClient,
    const char *mapName,
    char **errPtr
)
{
    assert(hazelcastClient != NULL);
    assert(hazelcastClient->client != NULL);
    assert(hazelcastClient->context != NULL);

    assert(mapName != NULL);

    try {
        RawDataIMapImpl mapImpl(mapName, hazelcastClient->context);
        return mapImpl.size();
    } catch(const std::runtime_error& re) {
        saveMessageInErrPtr(errPtr, re.what());
    } catch(const std::exception& ex) {
        saveMessageInErrPtr(errPtr, ex.what());
    } catch(...) {
        saveUnknownErrorOccurredMessageInErrPtr(errPtr);
    }

    return -1;
}

/* API functions */
extern "C" void Hazelcast_free(void *ptr)
{
    free(ptr);
}

/* Debug */
/*extern "C" void Hazelcast_test()
{
    hazelcast::client::ClientConfig config;
    hazelcast::client::HazelcastClient hz(config);

    std::cout << "######### Hello world\n";
}*/