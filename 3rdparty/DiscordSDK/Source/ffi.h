#ifndef _DISCORD_GAME_SDK_H_
#define _DISCORD_GAME_SDK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#define DISCORD_VERSION 2
#define DISCORD_APPLICATION_MANAGER_VERSION 1
#define DISCORD_USER_MANAGER_VERSION 1
#define DISCORD_IMAGE_MANAGER_VERSION 1
#define DISCORD_ACTIVITY_MANAGER_VERSION 1
#define DISCORD_RELATIONSHIP_MANAGER_VERSION 1
#define DISCORD_LOBBY_MANAGER_VERSION 1
#define DISCORD_NETWORK_MANAGER_VERSION 1
#define DISCORD_OVERLAY_MANAGER_VERSION 1
#define DISCORD_STORAGE_MANAGER_VERSION 1
#define DISCORD_STORE_MANAGER_VERSION 1
#define DISCORD_VOICE_MANAGER_VERSION 1
#define DISCORD_ACHIEVEMENT_MANAGER_VERSION 1

enum EDiscordResult {
    DiscordResult_Ok = 0,
    DiscordResult_ServiceUnavailable = 1,
    DiscordResult_InvalidVersion = 2,
    DiscordResult_LockFailed = 3,
    DiscordResult_InternalError = 4,
    DiscordResult_InvalidPayload = 5,
    DiscordResult_InvalidCommand = 6,
    DiscordResult_InvalidPermissions = 7,
    DiscordResult_NotFetched = 8,
    DiscordResult_NotFound = 9,
    DiscordResult_Conflict = 10,
    DiscordResult_InvalidSecret = 11,
    DiscordResult_InvalidJoinSecret = 12,
    DiscordResult_NoEligibleActivity = 13,
    DiscordResult_InvalidInvite = 14,
    DiscordResult_NotAuthenticated = 15,
    DiscordResult_InvalidAccessToken = 16,
    DiscordResult_ApplicationMismatch = 17,
    DiscordResult_InvalidDataUrl = 18,
    DiscordResult_InvalidBase64 = 19,
    DiscordResult_NotFiltered = 20,
    DiscordResult_LobbyFull = 21,
    DiscordResult_InvalidLobbySecret = 22,
    DiscordResult_InvalidFilename = 23,
    DiscordResult_InvalidFileSize = 24,
    DiscordResult_InvalidEntitlement = 25,
    DiscordResult_NotInstalled = 26,
    DiscordResult_NotRunning = 27,
    DiscordResult_InsufficientBuffer = 28,
    DiscordResult_PurchaseCanceled = 29,
    DiscordResult_InvalidGuild = 30,
    DiscordResult_InvalidEvent = 31,
    DiscordResult_InvalidChannel = 32,
    DiscordResult_InvalidOrigin = 33,
    DiscordResult_RateLimited = 34,
    DiscordResult_OAuth2Error = 35,
    DiscordResult_SelectChannelTimeout = 36,
    DiscordResult_GetGuildTimeout = 37,
    DiscordResult_SelectVoiceForceRequired = 38,
    DiscordResult_CaptureShortcutAlreadyListening = 39,
    DiscordResult_UnauthorizedForAchievement = 40,
    DiscordResult_InvalidGiftCode = 41,
    DiscordResult_PurchaseError = 42,
    DiscordResult_TransactionAborted = 43,
};

enum EDiscordCreateFlags {
    DiscordCreateFlags_Default = 0,
    DiscordCreateFlags_NoRequireDiscord = 1,
};

enum EDiscordLogLevel {
    DiscordLogLevel_Error = 1,
    DiscordLogLevel_Warn,
    DiscordLogLevel_Info,
    DiscordLogLevel_Debug,
};

enum EDiscordUserFlag {
    DiscordUserFlag_Partner = 2,
    DiscordUserFlag_HypeSquadEvents = 4,
    DiscordUserFlag_HypeSquadHouse1 = 64,
    DiscordUserFlag_HypeSquadHouse2 = 128,
    DiscordUserFlag_HypeSquadHouse3 = 256,
};

enum EDiscordPremiumType {
    DiscordPremiumType_None = 0,
    DiscordPremiumType_Tier1 = 1,
    DiscordPremiumType_Tier2 = 2,
};

enum EDiscordImageType {
    DiscordImageType_User,
};

enum EDiscordActivityType {
    DiscordActivityType_Playing,
    DiscordActivityType_Streaming,
    DiscordActivityType_Listening,
    DiscordActivityType_Watching,
};

enum EDiscordActivityActionType {
    DiscordActivityActionType_Join = 1,
    DiscordActivityActionType_Spectate,
};

enum EDiscordActivityJoinRequestReply {
    DiscordActivityJoinRequestReply_No,
    DiscordActivityJoinRequestReply_Yes,
    DiscordActivityJoinRequestReply_Ignore,
};

enum EDiscordStatus {
    DiscordStatus_Offline = 0,
    DiscordStatus_Online = 1,
    DiscordStatus_Idle = 2,
    DiscordStatus_DoNotDisturb = 3,
};

enum EDiscordRelationshipType {
    DiscordRelationshipType_None,
    DiscordRelationshipType_Friend,
    DiscordRelationshipType_Blocked,
    DiscordRelationshipType_PendingIncoming,
    DiscordRelationshipType_PendingOutgoing,
    DiscordRelationshipType_Implicit,
};

enum EDiscordLobbyType {
    DiscordLobbyType_Private = 1,
    DiscordLobbyType_Public,
};

enum EDiscordLobbySearchComparison {
    DiscordLobbySearchComparison_LessThanOrEqual = -2,
    DiscordLobbySearchComparison_LessThan,
    DiscordLobbySearchComparison_Equal,
    DiscordLobbySearchComparison_GreaterThan,
    DiscordLobbySearchComparison_GreaterThanOrEqual,
    DiscordLobbySearchComparison_NotEqual,
};

enum EDiscordLobbySearchCast {
    DiscordLobbySearchCast_String = 1,
    DiscordLobbySearchCast_Number,
};

enum EDiscordLobbySearchDistance {
    DiscordLobbySearchDistance_Local,
    DiscordLobbySearchDistance_Default,
    DiscordLobbySearchDistance_Extended,
    DiscordLobbySearchDistance_Global,
};

enum EDiscordEntitlementType {
    DiscordEntitlementType_Purchase = 1,
    DiscordEntitlementType_PremiumSubscription,
    DiscordEntitlementType_DeveloperGift,
    DiscordEntitlementType_TestModePurchase,
    DiscordEntitlementType_FreePurchase,
    DiscordEntitlementType_UserGift,
    DiscordEntitlementType_PremiumPurchase,
};

enum EDiscordSkuType {
    DiscordSkuType_Application = 1,
    DiscordSkuType_DLC,
    DiscordSkuType_Consumable,
    DiscordSkuType_Bundle,
};

enum EDiscordInputModeType {
    DiscordInputModeType_VoiceActivity = 0,
    DiscordInputModeType_PushToTalk,
};

typedef int64_t DiscordClientId;
typedef int32_t DiscordVersion;
typedef int64_t DiscordSnowflake;
typedef int64_t DiscordTimestamp;
typedef DiscordSnowflake DiscordUserId;
typedef char DiscordLocale[128];
typedef char DiscordBranch[4096];
typedef DiscordSnowflake DiscordLobbyId;
typedef char DiscordLobbySecret[128];
typedef char DiscordMetadataKey[256];
typedef char DiscordMetadataValue[4096];
typedef uint64_t DiscordNetworkPeerId;
typedef uint8_t DiscordNetworkChannelId;
typedef char DiscordPath[4096];
typedef char DiscordDateTime[64];

struct DiscordUser {
    DiscordUserId id;
    char username[256];
    char discriminator[8];
    char avatar[128];
    bool bot;
};

struct DiscordOAuth2Token {
    char access_token[128];
    char scopes[1024];
    DiscordTimestamp expires;
};

struct DiscordImageHandle {
    enum EDiscordImageType type;
    int64_t id;
    uint32_t size;
};

struct DiscordImageDimensions {
    uint32_t width;
    uint32_t height;
};

struct DiscordActivityTimestamps {
    DiscordTimestamp start;
    DiscordTimestamp end;
};

struct DiscordActivityAssets {
    char large_image[128];
    char large_text[128];
    char small_image[128];
    char small_text[128];
};

struct DiscordPartySize {
    int32_t current_size;
    int32_t max_size;
};

struct DiscordActivityParty {
    char id[128];
    struct DiscordPartySize size;
};

struct DiscordActivitySecrets {
    char match[128];
    char join[128];
    char spectate[128];
};

struct DiscordActivity {
    enum EDiscordActivityType type;
    int64_t application_id;
    char name[128];
    char state[128];
    char details[128];
    struct DiscordActivityTimestamps timestamps;
    struct DiscordActivityAssets assets;
    struct DiscordActivityParty party;
    struct DiscordActivitySecrets secrets;
    bool instance;
};

struct DiscordPresence {
    enum EDiscordStatus status;
    struct DiscordActivity activity;
};

struct DiscordRelationship {
    enum EDiscordRelationshipType type;
    struct DiscordUser user;
    struct DiscordPresence presence;
};

struct DiscordLobby {
    DiscordLobbyId id;
    enum EDiscordLobbyType type;
    DiscordUserId owner_id;
    DiscordLobbySecret secret;
    uint32_t capacity;
    bool locked;
};

struct DiscordFileStat {
    char filename[260];
    uint64_t size;
    uint64_t last_modified;
};

struct DiscordEntitlement {
    DiscordSnowflake id;
    enum EDiscordEntitlementType type;
    DiscordSnowflake sku_id;
};

struct DiscordSkuPrice {
    uint32_t amount;
    char currency[16];
};

struct DiscordSku {
    DiscordSnowflake id;
    enum EDiscordSkuType type;
    char name[256];
    struct DiscordSkuPrice price;
};

struct DiscordInputMode {
    enum EDiscordInputModeType type;
    char shortcut[256];
};

struct DiscordUserAchievement {
    DiscordSnowflake user_id;
    DiscordSnowflake achievement_id;
    uint8_t percent_complete;
    DiscordDateTime unlocked_at;
};

struct IDiscordLobbyTransaction {
    enum EDiscordResult (*set_type)(struct IDiscordLobbyTransaction* lobby_transaction,
                                    enum EDiscordLobbyType type);
    enum EDiscordResult (*set_owner)(struct IDiscordLobbyTransaction* lobby_transaction,
                                     DiscordUserId owner_id);
    enum EDiscordResult (*set_capacity)(struct IDiscordLobbyTransaction* lobby_transaction,
                                        uint32_t capacity);
    enum EDiscordResult (*set_metadata)(struct IDiscordLobbyTransaction* lobby_transaction,
                                        DiscordMetadataKey key,
                                        DiscordMetadataValue value);
    enum EDiscordResult (*delete_metadata)(struct IDiscordLobbyTransaction* lobby_transaction,
                                           DiscordMetadataKey key);
    enum EDiscordResult (*set_locked)(struct IDiscordLobbyTransaction* lobby_transaction,
                                      bool locked);
};

struct IDiscordLobbyMemberTransaction {
    enum EDiscordResult (*set_metadata)(
      struct IDiscordLobbyMemberTransaction* lobby_member_transaction,
      DiscordMetadataKey key,
      DiscordMetadataValue value);
    enum EDiscordResult (*delete_metadata)(
      struct IDiscordLobbyMemberTransaction* lobby_member_transaction,
      DiscordMetadataKey key);
};

struct IDiscordLobbySearchQuery {
    enum EDiscordResult (*filter)(struct IDiscordLobbySearchQuery* lobby_search_query,
                                  DiscordMetadataKey key,
                                  enum EDiscordLobbySearchComparison comparison,
                                  enum EDiscordLobbySearchCast cast,
                                  DiscordMetadataValue value);
    enum EDiscordResult (*sort)(struct IDiscordLobbySearchQuery* lobby_search_query,
                                DiscordMetadataKey key,
                                enum EDiscordLobbySearchCast cast,
                                DiscordMetadataValue value);
    enum EDiscordResult (*limit)(struct IDiscordLobbySearchQuery* lobby_search_query,
                                 uint32_t limit);
    enum EDiscordResult (*distance)(struct IDiscordLobbySearchQuery* lobby_search_query,
                                    enum EDiscordLobbySearchDistance distance);
};

typedef void* IDiscordApplicationEvents;

struct IDiscordApplicationManager {
    void (*validate_or_exit)(struct IDiscordApplicationManager* manager,
                             void* callback_data,
                             void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*get_current_locale)(struct IDiscordApplicationManager* manager, DiscordLocale* locale);
    void (*get_current_branch)(struct IDiscordApplicationManager* manager, DiscordBranch* branch);
    void (*get_oauth2_token)(struct IDiscordApplicationManager* manager,
                             void* callback_data,
                             void (*callback)(void* callback_data,
                                              enum EDiscordResult result,
                                              struct DiscordOAuth2Token* oauth2_token));
    void (*get_ticket)(struct IDiscordApplicationManager* manager,
                       void* callback_data,
                       void (*callback)(void* callback_data,
                                        enum EDiscordResult result,
                                        const char* data));
};

struct IDiscordUserEvents {
    void (*on_current_user_update)(void* event_data);
};

struct IDiscordUserManager {
    enum EDiscordResult (*get_current_user)(struct IDiscordUserManager* manager,
                                            struct DiscordUser* current_user);
    void (*get_user)(struct IDiscordUserManager* manager,
                     DiscordUserId user_id,
                     void* callback_data,
                     void (*callback)(void* callback_data,
                                      enum EDiscordResult result,
                                      struct DiscordUser* user));
    enum EDiscordResult (*get_current_user_premium_type)(struct IDiscordUserManager* manager,
                                                         enum EDiscordPremiumType* premium_type);
    enum EDiscordResult (*current_user_has_flag)(struct IDiscordUserManager* manager,
                                                 enum EDiscordUserFlag flag,
                                                 bool* has_flag);
};

typedef void* IDiscordImageEvents;

struct IDiscordImageManager {
    void (*fetch)(struct IDiscordImageManager* manager,
                  struct DiscordImageHandle handle,
                  bool refresh,
                  void* callback_data,
                  void (*callback)(void* callback_data,
                                   enum EDiscordResult result,
                                   struct DiscordImageHandle handle_result));
    enum EDiscordResult (*get_dimensions)(struct IDiscordImageManager* manager,
                                          struct DiscordImageHandle handle,
                                          struct DiscordImageDimensions* dimensions);
    enum EDiscordResult (*get_data)(struct IDiscordImageManager* manager,
                                    struct DiscordImageHandle handle,
                                    uint8_t* data,
                                    uint32_t data_length);
};

struct IDiscordActivityEvents {
    void (*on_activity_join)(void* event_data, const char* secret);
    void (*on_activity_spectate)(void* event_data, const char* secret);
    void (*on_activity_join_request)(void* event_data, struct DiscordUser* user);
    void (*on_activity_invite)(void* event_data,
                               enum EDiscordActivityActionType type,
                               struct DiscordUser* user,
                               struct DiscordActivity* activity);
};

struct IDiscordActivityManager {
    enum EDiscordResult (*register_command)(struct IDiscordActivityManager* manager,
                                            const char* command);
    enum EDiscordResult (*register_steam)(struct IDiscordActivityManager* manager,
                                          uint32_t steam_id);
    void (*update_activity)(struct IDiscordActivityManager* manager,
                            struct DiscordActivity* activity,
                            void* callback_data,
                            void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*clear_activity)(struct IDiscordActivityManager* manager,
                           void* callback_data,
                           void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*send_request_reply)(struct IDiscordActivityManager* manager,
                               DiscordUserId user_id,
                               enum EDiscordActivityJoinRequestReply reply,
                               void* callback_data,
                               void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*send_invite)(struct IDiscordActivityManager* manager,
                        DiscordUserId user_id,
                        enum EDiscordActivityActionType type,
                        const char* content,
                        void* callback_data,
                        void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*accept_invite)(struct IDiscordActivityManager* manager,
                          DiscordUserId user_id,
                          void* callback_data,
                          void (*callback)(void* callback_data, enum EDiscordResult result));
};

struct IDiscordRelationshipEvents {
    void (*on_refresh)(void* event_data);
    void (*on_relationship_update)(void* event_data, struct DiscordRelationship* relationship);
};

struct IDiscordRelationshipManager {
    void (*filter)(struct IDiscordRelationshipManager* manager,
                   void* filter_data,
                   bool (*filter)(void* filter_data, struct DiscordRelationship* relationship));
    enum EDiscordResult (*count)(struct IDiscordRelationshipManager* manager, int32_t* count);
    enum EDiscordResult (*get)(struct IDiscordRelationshipManager* manager,
                               DiscordUserId user_id,
                               struct DiscordRelationship* relationship);
    enum EDiscordResult (*get_at)(struct IDiscordRelationshipManager* manager,
                                  uint32_t index,
                                  struct DiscordRelationship* relationship);
};

struct IDiscordLobbyEvents {
    void (*on_lobby_update)(void* event_data, int64_t lobby_id);
    void (*on_lobby_delete)(void* event_data, int64_t lobby_id, uint32_t reason);
    void (*on_member_connect)(void* event_data, int64_t lobby_id, int64_t user_id);
    void (*on_member_update)(void* event_data, int64_t lobby_id, int64_t user_id);
    void (*on_member_disconnect)(void* event_data, int64_t lobby_id, int64_t user_id);
    void (*on_lobby_message)(void* event_data,
                             int64_t lobby_id,
                             int64_t user_id,
                             uint8_t* data,
                             uint32_t data_length);
    void (*on_speaking)(void* event_data, int64_t lobby_id, int64_t user_id, bool speaking);
    void (*on_network_message)(void* event_data,
                               int64_t lobby_id,
                               int64_t user_id,
                               uint8_t channel_id,
                               uint8_t* data,
                               uint32_t data_length);
};

struct IDiscordLobbyManager {
    enum EDiscordResult (*get_lobby_create_transaction)(
      struct IDiscordLobbyManager* manager,
      struct IDiscordLobbyTransaction** transaction);
    enum EDiscordResult (*get_lobby_update_transaction)(
      struct IDiscordLobbyManager* manager,
      DiscordLobbyId lobby_id,
      struct IDiscordLobbyTransaction** transaction);
    enum EDiscordResult (*get_member_update_transaction)(
      struct IDiscordLobbyManager* manager,
      DiscordLobbyId lobby_id,
      DiscordUserId user_id,
      struct IDiscordLobbyMemberTransaction** transaction);
    void (*create_lobby)(struct IDiscordLobbyManager* manager,
                         struct IDiscordLobbyTransaction* transaction,
                         void* callback_data,
                         void (*callback)(void* callback_data,
                                          enum EDiscordResult result,
                                          struct DiscordLobby* lobby));
    void (*update_lobby)(struct IDiscordLobbyManager* manager,
                         DiscordLobbyId lobby_id,
                         struct IDiscordLobbyTransaction* transaction,
                         void* callback_data,
                         void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*delete_lobby)(struct IDiscordLobbyManager* manager,
                         DiscordLobbyId lobby_id,
                         void* callback_data,
                         void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*connect_lobby)(struct IDiscordLobbyManager* manager,
                          DiscordLobbyId lobby_id,
                          DiscordLobbySecret secret,
                          void* callback_data,
                          void (*callback)(void* callback_data,
                                           enum EDiscordResult result,
                                           struct DiscordLobby* lobby));
    void (*connect_lobby_with_activity_secret)(struct IDiscordLobbyManager* manager,
                                               DiscordLobbySecret activity_secret,
                                               void* callback_data,
                                               void (*callback)(void* callback_data,
                                                                enum EDiscordResult result,
                                                                struct DiscordLobby* lobby));
    void (*disconnect_lobby)(struct IDiscordLobbyManager* manager,
                             DiscordLobbyId lobby_id,
                             void* callback_data,
                             void (*callback)(void* callback_data, enum EDiscordResult result));
    enum EDiscordResult (*get_lobby)(struct IDiscordLobbyManager* manager,
                                     DiscordLobbyId lobby_id,
                                     struct DiscordLobby* lobby);
    enum EDiscordResult (*get_lobby_activity_secret)(struct IDiscordLobbyManager* manager,
                                                     DiscordLobbyId lobby_id,
                                                     DiscordLobbySecret* secret);
    enum EDiscordResult (*get_lobby_metadata_value)(struct IDiscordLobbyManager* manager,
                                                    DiscordLobbyId lobby_id,
                                                    DiscordMetadataKey key,
                                                    DiscordMetadataValue* value);
    enum EDiscordResult (*get_lobby_metadata_key)(struct IDiscordLobbyManager* manager,
                                                  DiscordLobbyId lobby_id,
                                                  int32_t index,
                                                  DiscordMetadataKey* key);
    enum EDiscordResult (*lobby_metadata_count)(struct IDiscordLobbyManager* manager,
                                                DiscordLobbyId lobby_id,
                                                int32_t* count);
    enum EDiscordResult (*member_count)(struct IDiscordLobbyManager* manager,
                                        DiscordLobbyId lobby_id,
                                        int32_t* count);
    enum EDiscordResult (*get_member_user_id)(struct IDiscordLobbyManager* manager,
                                              DiscordLobbyId lobby_id,
                                              int32_t index,
                                              DiscordUserId* user_id);
    enum EDiscordResult (*get_member_user)(struct IDiscordLobbyManager* manager,
                                           DiscordLobbyId lobby_id,
                                           DiscordUserId user_id,
                                           struct DiscordUser* user);
    enum EDiscordResult (*get_member_metadata_value)(struct IDiscordLobbyManager* manager,
                                                     DiscordLobbyId lobby_id,
                                                     DiscordUserId user_id,
                                                     DiscordMetadataKey key,
                                                     DiscordMetadataValue* value);
    enum EDiscordResult (*get_member_metadata_key)(struct IDiscordLobbyManager* manager,
                                                   DiscordLobbyId lobby_id,
                                                   DiscordUserId user_id,
                                                   int32_t index,
                                                   DiscordMetadataKey* key);
    enum EDiscordResult (*member_metadata_count)(struct IDiscordLobbyManager* manager,
                                                 DiscordLobbyId lobby_id,
                                                 DiscordUserId user_id,
                                                 int32_t* count);
    void (*update_member)(struct IDiscordLobbyManager* manager,
                          DiscordLobbyId lobby_id,
                          DiscordUserId user_id,
                          struct IDiscordLobbyMemberTransaction* transaction,
                          void* callback_data,
                          void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*send_lobby_message)(struct IDiscordLobbyManager* manager,
                               DiscordLobbyId lobby_id,
                               uint8_t* data,
                               uint32_t data_length,
                               void* callback_data,
                               void (*callback)(void* callback_data, enum EDiscordResult result));
    enum EDiscordResult (*get_search_query)(struct IDiscordLobbyManager* manager,
                                            struct IDiscordLobbySearchQuery** query);
    void (*search)(struct IDiscordLobbyManager* manager,
                   struct IDiscordLobbySearchQuery* query,
                   void* callback_data,
                   void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*lobby_count)(struct IDiscordLobbyManager* manager, int32_t* count);
    enum EDiscordResult (*get_lobby_id)(struct IDiscordLobbyManager* manager,
                                        int32_t index,
                                        DiscordLobbyId* lobby_id);
    void (*connect_voice)(struct IDiscordLobbyManager* manager,
                          DiscordLobbyId lobby_id,
                          void* callback_data,
                          void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*disconnect_voice)(struct IDiscordLobbyManager* manager,
                             DiscordLobbyId lobby_id,
                             void* callback_data,
                             void (*callback)(void* callback_data, enum EDiscordResult result));
    enum EDiscordResult (*connect_network)(struct IDiscordLobbyManager* manager,
                                           DiscordLobbyId lobby_id);
    enum EDiscordResult (*disconnect_network)(struct IDiscordLobbyManager* manager,
                                              DiscordLobbyId lobby_id);
    enum EDiscordResult (*flush_network)(struct IDiscordLobbyManager* manager);
    enum EDiscordResult (*open_network_channel)(struct IDiscordLobbyManager* manager,
                                                DiscordLobbyId lobby_id,
                                                uint8_t channel_id,
                                                bool reliable);
    enum EDiscordResult (*send_network_message)(struct IDiscordLobbyManager* manager,
                                                DiscordLobbyId lobby_id,
                                                DiscordUserId user_id,
                                                uint8_t channel_id,
                                                uint8_t* data,
                                                uint32_t data_length);
};

struct IDiscordNetworkEvents {
    void (*on_message)(void* event_data,
                       DiscordNetworkPeerId peer_id,
                       DiscordNetworkChannelId channel_id,
                       uint8_t* data,
                       uint32_t data_length);
    void (*on_route_update)(void* event_data, const char* route_data);
};

struct IDiscordNetworkManager {
    /**
     * Get the local peer ID for this process.
     */
    void (*get_peer_id)(struct IDiscordNetworkManager* manager, DiscordNetworkPeerId* peer_id);
    /**
     * Send pending network messages.
     */
    enum EDiscordResult (*flush)(struct IDiscordNetworkManager* manager);
    /**
     * Open a connection to a remote peer.
     */
    enum EDiscordResult (*open_peer)(struct IDiscordNetworkManager* manager,
                                     DiscordNetworkPeerId peer_id,
                                     const char* route_data);
    /**
     * Update the route data for a connected peer.
     */
    enum EDiscordResult (*update_peer)(struct IDiscordNetworkManager* manager,
                                       DiscordNetworkPeerId peer_id,
                                       const char* route_data);
    /**
     * Close the connection to a remote peer.
     */
    enum EDiscordResult (*close_peer)(struct IDiscordNetworkManager* manager,
                                      DiscordNetworkPeerId peer_id);
    /**
     * Open a message channel to a connected peer.
     */
    enum EDiscordResult (*open_channel)(struct IDiscordNetworkManager* manager,
                                        DiscordNetworkPeerId peer_id,
                                        DiscordNetworkChannelId channel_id,
                                        bool reliable);
    /**
     * Close a message channel to a connected peer.
     */
    enum EDiscordResult (*close_channel)(struct IDiscordNetworkManager* manager,
                                         DiscordNetworkPeerId peer_id,
                                         DiscordNetworkChannelId channel_id);
    /**
     * Send a message to a connected peer over an opened message channel.
     */
    enum EDiscordResult (*send_message)(struct IDiscordNetworkManager* manager,
                                        DiscordNetworkPeerId peer_id,
                                        DiscordNetworkChannelId channel_id,
                                        uint8_t* data,
                                        uint32_t data_length);
};

struct IDiscordOverlayEvents {
    void (*on_toggle)(void* event_data, bool locked);
};

struct IDiscordOverlayManager {
    void (*is_enabled)(struct IDiscordOverlayManager* manager, bool* enabled);
    void (*is_locked)(struct IDiscordOverlayManager* manager, bool* locked);
    void (*set_locked)(struct IDiscordOverlayManager* manager,
                       bool locked,
                       void* callback_data,
                       void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*open_activity_invite)(struct IDiscordOverlayManager* manager,
                                 enum EDiscordActivityActionType type,
                                 void* callback_data,
                                 void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*open_guild_invite)(struct IDiscordOverlayManager* manager,
                              const char* code,
                              void* callback_data,
                              void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*open_voice_settings)(struct IDiscordOverlayManager* manager,
                                void* callback_data,
                                void (*callback)(void* callback_data, enum EDiscordResult result));
};

typedef void* IDiscordStorageEvents;

struct IDiscordStorageManager {
    enum EDiscordResult (*read)(struct IDiscordStorageManager* manager,
                                const char* name,
                                uint8_t* data,
                                uint32_t data_length,
                                uint32_t* read);
    void (*read_async)(struct IDiscordStorageManager* manager,
                       const char* name,
                       void* callback_data,
                       void (*callback)(void* callback_data,
                                        enum EDiscordResult result,
                                        uint8_t* data,
                                        uint32_t data_length));
    void (*read_async_partial)(struct IDiscordStorageManager* manager,
                               const char* name,
                               uint64_t offset,
                               uint64_t length,
                               void* callback_data,
                               void (*callback)(void* callback_data,
                                                enum EDiscordResult result,
                                                uint8_t* data,
                                                uint32_t data_length));
    enum EDiscordResult (*write)(struct IDiscordStorageManager* manager,
                                 const char* name,
                                 uint8_t* data,
                                 uint32_t data_length);
    void (*write_async)(struct IDiscordStorageManager* manager,
                        const char* name,
                        uint8_t* data,
                        uint32_t data_length,
                        void* callback_data,
                        void (*callback)(void* callback_data, enum EDiscordResult result));
    enum EDiscordResult (*delete_)(struct IDiscordStorageManager* manager, const char* name);
    enum EDiscordResult (*exists)(struct IDiscordStorageManager* manager,
                                  const char* name,
                                  bool* exists);
    void (*count)(struct IDiscordStorageManager* manager, int32_t* count);
    enum EDiscordResult (*stat)(struct IDiscordStorageManager* manager,
                                const char* name,
                                struct DiscordFileStat* stat);
    enum EDiscordResult (*stat_at)(struct IDiscordStorageManager* manager,
                                   int32_t index,
                                   struct DiscordFileStat* stat);
    enum EDiscordResult (*get_path)(struct IDiscordStorageManager* manager, DiscordPath* path);
};

struct IDiscordStoreEvents {
    void (*on_entitlement_create)(void* event_data, struct DiscordEntitlement* entitlement);
    void (*on_entitlement_delete)(void* event_data, struct DiscordEntitlement* entitlement);
};

struct IDiscordStoreManager {
    void (*fetch_skus)(struct IDiscordStoreManager* manager,
                       void* callback_data,
                       void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*count_skus)(struct IDiscordStoreManager* manager, int32_t* count);
    enum EDiscordResult (*get_sku)(struct IDiscordStoreManager* manager,
                                   DiscordSnowflake sku_id,
                                   struct DiscordSku* sku);
    enum EDiscordResult (*get_sku_at)(struct IDiscordStoreManager* manager,
                                      int32_t index,
                                      struct DiscordSku* sku);
    void (*fetch_entitlements)(struct IDiscordStoreManager* manager,
                               void* callback_data,
                               void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*count_entitlements)(struct IDiscordStoreManager* manager, int32_t* count);
    enum EDiscordResult (*get_entitlement)(struct IDiscordStoreManager* manager,
                                           DiscordSnowflake entitlement_id,
                                           struct DiscordEntitlement* entitlement);
    enum EDiscordResult (*get_entitlement_at)(struct IDiscordStoreManager* manager,
                                              int32_t index,
                                              struct DiscordEntitlement* entitlement);
    enum EDiscordResult (*has_sku_entitlement)(struct IDiscordStoreManager* manager,
                                               DiscordSnowflake sku_id,
                                               bool* has_entitlement);
    void (*start_purchase)(struct IDiscordStoreManager* manager,
                           DiscordSnowflake sku_id,
                           void* callback_data,
                           void (*callback)(void* callback_data, enum EDiscordResult result));
};

struct IDiscordVoiceEvents {
    void (*on_settings_update)(void* event_data);
};

struct IDiscordVoiceManager {
    enum EDiscordResult (*get_input_mode)(struct IDiscordVoiceManager* manager,
                                          struct DiscordInputMode* input_mode);
    void (*set_input_mode)(struct IDiscordVoiceManager* manager,
                           struct DiscordInputMode input_mode,
                           void* callback_data,
                           void (*callback)(void* callback_data, enum EDiscordResult result));
    enum EDiscordResult (*is_self_mute)(struct IDiscordVoiceManager* manager, bool* mute);
    enum EDiscordResult (*set_self_mute)(struct IDiscordVoiceManager* manager, bool mute);
    enum EDiscordResult (*is_self_deaf)(struct IDiscordVoiceManager* manager, bool* deaf);
    enum EDiscordResult (*set_self_deaf)(struct IDiscordVoiceManager* manager, bool deaf);
    enum EDiscordResult (*is_local_mute)(struct IDiscordVoiceManager* manager,
                                         DiscordSnowflake user_id,
                                         bool* mute);
    enum EDiscordResult (*set_local_mute)(struct IDiscordVoiceManager* manager,
                                          DiscordSnowflake user_id,
                                          bool mute);
    enum EDiscordResult (*get_local_volume)(struct IDiscordVoiceManager* manager,
                                            DiscordSnowflake user_id,
                                            uint8_t* volume);
    enum EDiscordResult (*set_local_volume)(struct IDiscordVoiceManager* manager,
                                            DiscordSnowflake user_id,
                                            uint8_t volume);
};

struct IDiscordAchievementEvents {
    void (*on_user_achievement_update)(void* event_data,
                                       struct DiscordUserAchievement* user_achievement);
};

struct IDiscordAchievementManager {
    void (*set_user_achievement)(struct IDiscordAchievementManager* manager,
                                 DiscordSnowflake achievement_id,
                                 uint8_t percent_complete,
                                 void* callback_data,
                                 void (*callback)(void* callback_data, enum EDiscordResult result));
    void (*fetch_user_achievements)(struct IDiscordAchievementManager* manager,
                                    void* callback_data,
                                    void (*callback)(void* callback_data,
                                                     enum EDiscordResult result));
    void (*count_user_achievements)(struct IDiscordAchievementManager* manager, int32_t* count);
    enum EDiscordResult (*get_user_achievement)(struct IDiscordAchievementManager* manager,
                                                DiscordSnowflake user_achievement_id,
                                                struct DiscordUserAchievement* user_achievement);
    enum EDiscordResult (*get_user_achievement_at)(struct IDiscordAchievementManager* manager,
                                                   int32_t index,
                                                   struct DiscordUserAchievement* user_achievement);
};

typedef void* IDiscordCoreEvents;

struct IDiscordCore {
    void (*destroy)(struct IDiscordCore* core);
    enum EDiscordResult (*run_callbacks)(struct IDiscordCore* core);
    void (*set_log_hook)(struct IDiscordCore* core,
                         enum EDiscordLogLevel min_level,
                         void* hook_data,
                         void (*hook)(void* hook_data,
                                      enum EDiscordLogLevel level,
                                      const char* message));
    struct IDiscordApplicationManager* (*get_application_manager)(struct IDiscordCore* core);
    struct IDiscordUserManager* (*get_user_manager)(struct IDiscordCore* core);
    struct IDiscordImageManager* (*get_image_manager)(struct IDiscordCore* core);
    struct IDiscordActivityManager* (*get_activity_manager)(struct IDiscordCore* core);
    struct IDiscordRelationshipManager* (*get_relationship_manager)(struct IDiscordCore* core);
    struct IDiscordLobbyManager* (*get_lobby_manager)(struct IDiscordCore* core);
    struct IDiscordNetworkManager* (*get_network_manager)(struct IDiscordCore* core);
    struct IDiscordOverlayManager* (*get_overlay_manager)(struct IDiscordCore* core);
    struct IDiscordStorageManager* (*get_storage_manager)(struct IDiscordCore* core);
    struct IDiscordStoreManager* (*get_store_manager)(struct IDiscordCore* core);
    struct IDiscordVoiceManager* (*get_voice_manager)(struct IDiscordCore* core);
    struct IDiscordAchievementManager* (*get_achievement_manager)(struct IDiscordCore* core);
};

struct DiscordCreateParams {
    DiscordClientId client_id;
    uint64_t flags;
    IDiscordCoreEvents* events;
    void* event_data;
    IDiscordApplicationEvents* application_events;
    DiscordVersion application_version;
    struct IDiscordUserEvents* user_events;
    DiscordVersion user_version;
    IDiscordImageEvents* image_events;
    DiscordVersion image_version;
    struct IDiscordActivityEvents* activity_events;
    DiscordVersion activity_version;
    struct IDiscordRelationshipEvents* relationship_events;
    DiscordVersion relationship_version;
    struct IDiscordLobbyEvents* lobby_events;
    DiscordVersion lobby_version;
    struct IDiscordNetworkEvents* network_events;
    DiscordVersion network_version;
    struct IDiscordOverlayEvents* overlay_events;
    DiscordVersion overlay_version;
    IDiscordStorageEvents* storage_events;
    DiscordVersion storage_version;
    struct IDiscordStoreEvents* store_events;
    DiscordVersion store_version;
    struct IDiscordVoiceEvents* voice_events;
    DiscordVersion voice_version;
    struct IDiscordAchievementEvents* achievement_events;
    DiscordVersion achievement_version;
};

#ifdef __cplusplus
inline
#else
static
#endif
  void
  DiscordCreateParamsSetDefault(struct DiscordCreateParams* params)
{
    memset(params, 0, sizeof(struct DiscordCreateParams));
    params->application_version = DISCORD_APPLICATION_MANAGER_VERSION;
    params->user_version = DISCORD_USER_MANAGER_VERSION;
    params->image_version = DISCORD_IMAGE_MANAGER_VERSION;
    params->activity_version = DISCORD_ACTIVITY_MANAGER_VERSION;
    params->relationship_version = DISCORD_RELATIONSHIP_MANAGER_VERSION;
    params->lobby_version = DISCORD_LOBBY_MANAGER_VERSION;
    params->network_version = DISCORD_NETWORK_MANAGER_VERSION;
    params->overlay_version = DISCORD_OVERLAY_MANAGER_VERSION;
    params->storage_version = DISCORD_STORAGE_MANAGER_VERSION;
    params->store_version = DISCORD_STORE_MANAGER_VERSION;
    params->voice_version = DISCORD_VOICE_MANAGER_VERSION;
    params->achievement_version = DISCORD_ACHIEVEMENT_MANAGER_VERSION;
}

enum EDiscordResult DiscordCreate(DiscordVersion version,
                                  struct DiscordCreateParams* params,
                                  struct IDiscordCore** result);

#ifdef __cplusplus
}
#endif

#endif