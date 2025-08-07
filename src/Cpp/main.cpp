#pragma warning(disable: 4996)  

#define IMGUI_USE_STD_STRING
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"

#include <d3d11.h>
#include <tchar.h>
#include <string>
#include <Windows.h>

#include <filesystem>
#include <stdexcept>

#include <opencv2/opencv.hpp>
#include <nlohmann/json.hpp>

#include <fstream>
#include <vector>
#include <map>
#include <cstring>
#include <iomanip>

#include "main.h"

#include "AsstCaller.h"

// 确保已链接这些库
#pragma comment(lib, "opencv_world4.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

using json = nlohmann::json;

// 字符串缓冲区大小定义
const int STR_BUFFER_SIZE = 256;
const int STAGE_BUFFER_SIZE = 128;
const int ACCOUNT_BUFFER_SIZE = 128;
const int PENGUIN_ID_BUFFER_SIZE = 64;
const int YITULIU_ID_BUFFER_SIZE = 64;
const int TAG_BUFFER_SIZE = 128;
const int FACILITY_BUFFER_SIZE = 64;  // 设施名称缓冲区大小

// 第一份配置：启动参数
struct StartupConfig {
    bool enable = true;
    char client_type[STR_BUFFER_SIZE] = { 0 };
    bool start_game_enabled = true;
    char account_name[ACCOUNT_BUFFER_SIZE] = { 0 };
};

// 第二份配置：关卡任务参数
struct StageConfig {
    bool enable = true;
    char stage[STAGE_BUFFER_SIZE] = { 0 };
    int medicine = 0;
    int expiring_medicine = 0;
    int stone = 0;
    int times = 255;
    int series = -1;
    std::map<std::string, int> drops;
    bool report_to_penguin = false;
    char penguin_id[PENGUIN_ID_BUFFER_SIZE] = { 0 };
    char server[STR_BUFFER_SIZE] = "CN";
    char client_type[STR_BUFFER_SIZE] = { 0 };
    bool DrGrandet = false;
};

// 第三份配置：公招参数
struct RecruitmentConfig {
    bool enable = true;
    bool refresh = true;  // 永远为true且不提供设置
    std::vector<int> select = { 3, 4 };  // 必选，默认3,4
    std::vector<int> confirm = { 3, 4 };  // 必选，默认3,4
    std::vector<std::string> first_tags;  // 留空
    int extra_tags_mode = 0;  // 0-默认，1-选3个，2-更多高星组合
    int times = 3;  // 默认3
    bool set_time = true;  // 永远为true且不提供设置
    bool expedite = false;
    int expedite_times = 0;  // 加急次数，expedite为true时有效
    bool skip_robot = true;  // 默认跳过
    std::map<std::string, int> recruitment_time;  // 空数组
    bool report_to_penguin = false;
    char penguin_id[PENGUIN_ID_BUFFER_SIZE] = { 0 };
    bool report_to_yituliu = false;
    char yituliu_id[YITULIU_ID_BUFFER_SIZE] = { 0 };
    char server[STR_BUFFER_SIZE] = "CN";
    char client_type[STR_BUFFER_SIZE] = { 0 };
};

// 第四份配置：设施换班参数
struct FacilityConfig {
    bool enable = true;
    int mode = 0;  // 模式
    std::vector<std::string> facility;  // 要换班的设施（有序）
    std::string drones = "_NotUse";  // 无人机用途
    float threshold = 0.3f;  // 工作心情阈值
    bool replenish = false;  // 贸易站自动补货
    bool dorm_notstationed_enabled = false;  // 宿舍“未进驻”选项
    bool dorm_trust_enabled = false;  // 宿舍填入信赖未满干员
};

// 新增：商店购物配置
struct ShoppingConfig {
    bool enable = true;                // 是否启用本任务
    bool shopping = true;              // 是否购物
    bool only_buy_discount = false;    // 是否只购买折扣物品
    bool reserve_max_credit = false;   // 信用点低于300时停止购买
};

// 新增：任务奖励配置
struct MissionConfig {
    bool enable = true;     // 是否启用本任务
    bool award = true;      // 领取每日/每周任务奖励
};

// 嵌套结构体：奖励配置列表
struct CollectibleStartList {
    bool hot_water = false;    // 热水壶奖励（通用）
    bool shield = false;       // 护盾奖励（通用）
    bool ingot = false;        // 源石锭奖励（通用）
    bool hope = false;         // 希望奖励（JieGarden主题不可用）
    bool random = false;       // 随机奖励（通用）
    bool key = false;          // 钥匙奖励（仅Mizuki）
    bool dice = false;         // 骰子奖励（仅Mizuki）
    bool ideas = false;        // 2构想奖励（仅Sarkaz）
};

// 肉鸽模式主配置结构体
struct RoguelikeConfig {
    // 基础配置
    bool enable = true;                          // 是否启用本任务
    char theme[32] = "Phantom";                  // 主题（C风格字符串）
    int mode = 0;                                // 模式
    char squad[128] = "指挥分队";                // 开局分队名（C风格字符串）
    char roles[128] = "先手必胜";                // 开局职业组（C风格字符串）
    char core_char[128] = "";                    // 开局干员名（C风格字符串）
    bool use_support = false;                    // 是否使用助战干员
    bool use_nonfriend_support = false;          // 是否允许非好友助战

    // 运行控制
    int starts_count = 999;                      // 开始探索次数
    int difficulty = 0;                          // 指定难度等级（除Phantom外有效）
    bool stop_at_final_boss = false;             // 第五层前停止（除Phantom外有效）
    bool stop_at_max_level = false;              // 等级满后停止

    // 投资相关
    bool investment_enabled = true;              // 是否投资源石锭
    int investments_count = INT_MAX;             // 投资次数
    bool stop_when_investment_full = false;      // 投资上限后停止
    bool investment_with_more_score = false;     // 投资后尝试购物（仅模式1）

    // 凹开局相关（仅模式4有效）
    bool start_with_elite_two = false;           // 凹干员精二直升
    bool only_start_with_elite_two = false;      // 只凹精二直升（需start_with_elite_two为true）
    CollectibleStartList collectible_mode_start_list;  // 凹开局期望奖励

    // 主题专属配置
    bool refresh_trader_with_dice = false;       // 用骰子刷新商店（仅Mizuki）
    bool use_foldartal = true;                   // 使用密文板（仅Sami）
    bool check_collapsal_paradigms = false;      // 检测坍缩范式（仅Sami）
    bool double_check_collapsal_paradigms = false; // 双重检测（仅Sami且check_collapsal_paradigms为true）

    // 自动切换设置
    bool monthly_squad_auto_iterate = false;     // 月度小队自动切换
    bool monthly_squad_check_comms = false;      // 月度小队通信切换依据
    bool deep_exploration_auto_iterate = false;  // 深入调查自动切换

    // 烧水相关配置
    bool collectible_mode_shopping = false;      // 烧水启用购物
    char collectible_mode_squad[128] = "";    // 烧水使用的分队
    CollectibleStartList collectible_mode_collectibles;  // 烧水期望奖励
};


// JSON序列化/反序列化
namespace nlohmann {
    // 启动配置序列化
    template <>
    struct adl_serializer<StartupConfig> {
        static void to_json(json& j, const StartupConfig& cfg) {
            j = json{
                {"enable", cfg.enable},
                {"client_type", cfg.client_type},
                {"start_game_enabled", cfg.start_game_enabled},
                {"account_name", cfg.account_name}
            };
        }
        static void from_json(const json& j, StartupConfig& cfg) {
            if (j.contains("enable")) j["enable"].get_to(cfg.enable);
            if (j.contains("client_type")) strncpy(cfg.client_type, j["client_type"].get<std::string>().c_str(), STR_BUFFER_SIZE - 1);
            if (j.contains("start_game_enabled")) j["start_game_enabled"].get_to(cfg.start_game_enabled);
            if (j.contains("account_name")) strncpy(cfg.account_name, j["account_name"].get<std::string>().c_str(), ACCOUNT_BUFFER_SIZE - 1);
        }
    };

    // 关卡配置序列化
    template <>
    struct adl_serializer<StageConfig> {
        static void to_json(json& j, const StageConfig& cfg) {
            j = json{
                {"enable", cfg.enable},
                {"stage", cfg.stage},
                {"medicine", cfg.medicine},
                {"expiring_medicine", cfg.expiring_medicine},
                {"stone", cfg.stone},
                {"times", cfg.times},
                {"series", cfg.series},
                {"drops", cfg.drops},
                {"report_to_penguin", cfg.report_to_penguin},
                {"penguin_id", cfg.penguin_id},
                {"server", cfg.server},
                {"client_type", cfg.client_type},
                {"DrGrandet", cfg.DrGrandet}
            };
        }
        static void from_json(const json& j, StageConfig& cfg) {
            if (j.contains("enable")) j["enable"].get_to(cfg.enable);
            if (j.contains("stage")) strncpy(cfg.stage, j["stage"].get<std::string>().c_str(), STAGE_BUFFER_SIZE - 1);
            if (j.contains("medicine")) j["medicine"].get_to(cfg.medicine);
            if (j.contains("expiring_medicine")) j["expiring_medicine"].get_to(cfg.expiring_medicine);
            if (j.contains("stone")) j["stone"].get_to(cfg.stone);
            if (j.contains("times")) j["times"].get_to(cfg.times);
            if (j.contains("series")) j["series"].get_to(cfg.series);
            if (j.contains("drops")) cfg.drops = j["drops"].get<std::map<std::string, int>>();
            if (j.contains("report_to_penguin")) j["report_to_penguin"].get_to(cfg.report_to_penguin);
            if (j.contains("penguin_id")) strncpy(cfg.penguin_id, j["penguin_id"].get<std::string>().c_str(), PENGUIN_ID_BUFFER_SIZE - 1);
            if (j.contains("server")) strncpy(cfg.server, j["server"].get<std::string>().c_str(), STR_BUFFER_SIZE - 1);
            if (j.contains("client_type")) strncpy(cfg.client_type, j["client_type"].get<std::string>().c_str(), STR_BUFFER_SIZE - 1);
            if (j.contains("DrGrandet")) j["DrGrandet"].get_to(cfg.DrGrandet);
        }
    };

    // 公招配置序列化
    template <>
    struct adl_serializer<RecruitmentConfig> {
        static void to_json(json& j, const RecruitmentConfig& cfg) {
            j = json{
                {"enable", cfg.enable},
                {"refresh", cfg.refresh},
                {"select", cfg.select},
                {"confirm", cfg.confirm},
                {"first_tags", cfg.first_tags},
                {"extra_tags_mode", cfg.extra_tags_mode},
                {"times", cfg.times},
                {"set_time", cfg.set_time},
                {"expedite", cfg.expedite},
                {"expedite_times", cfg.expedite_times},
                {"skip_robot", cfg.skip_robot},
                {"recruitment_time", cfg.recruitment_time},
                {"report_to_penguin", cfg.report_to_penguin},
                {"penguin_id", cfg.penguin_id},
                {"report_to_yituliu", cfg.report_to_yituliu},
                {"yituliu_id", cfg.yituliu_id},
                {"server", cfg.server},
                {"client_type", cfg.client_type}
            };
        }
        static void from_json(const json& j, RecruitmentConfig& cfg) {
            if (j.contains("enable")) j["enable"].get_to(cfg.enable);
            cfg.refresh = true;  // 强制为true
            if (j.contains("select")) cfg.select = j["select"].get<std::vector<int>>();
            if (j.contains("confirm")) cfg.confirm = j["confirm"].get<std::vector<int>>();
            if (j.contains("first_tags")) cfg.first_tags = j["first_tags"].get<std::vector<std::string>>();
            if (j.contains("extra_tags_mode")) j["extra_tags_mode"].get_to(cfg.extra_tags_mode);
            if (j.contains("times")) j["times"].get_to(cfg.times);
            cfg.set_time = true;  // 强制为true
            if (j.contains("expedite")) j["expedite"].get_to(cfg.expedite);
            if (j.contains("expedite_times")) j["expedite_times"].get_to(cfg.expedite_times);
            if (j.contains("skip_robot")) j["skip_robot"].get_to(cfg.skip_robot);
            cfg.recruitment_time = {};  // 强制为空
            if (j.contains("report_to_penguin")) j["report_to_penguin"].get_to(cfg.report_to_penguin);
            if (j.contains("penguin_id")) strncpy(cfg.penguin_id, j["penguin_id"].get<std::string>().c_str(), PENGUIN_ID_BUFFER_SIZE - 1);
            if (j.contains("report_to_yituliu")) j["report_to_yituliu"].get_to(cfg.report_to_yituliu);
            if (j.contains("yituliu_id")) strncpy(cfg.yituliu_id, j["yituliu_id"].get<std::string>().c_str(), YITULIU_ID_BUFFER_SIZE - 1);
            if (j.contains("server")) strncpy(cfg.server, j["server"].get<std::string>().c_str(), STR_BUFFER_SIZE - 1);
            if (j.contains("client_type")) strncpy(cfg.client_type, j["client_type"].get<std::string>().c_str(), STR_BUFFER_SIZE - 1);
        }
    };

    // 设施配置序列化
    template <>
    struct adl_serializer<FacilityConfig> {
        static void to_json(json& j, const FacilityConfig& cfg) {
            j = json{
                {"enable", cfg.enable},
                {"mode", cfg.mode},
                {"facility", cfg.facility},
                {"drones", cfg.drones},
                {"threshold", cfg.threshold},
                {"replenish", cfg.replenish},
                {"dorm_notstationed_enabled", cfg.dorm_notstationed_enabled},
                {"dorm_trust_enabled", cfg.dorm_trust_enabled}
            };
        }
        static void from_json(const json& j, FacilityConfig& cfg) {
            if (j.contains("enable")) j["enable"].get_to(cfg.enable);
            if (j.contains("mode")) j["mode"].get_to(cfg.mode);
            if (j.contains("facility")) cfg.facility = j["facility"].get<std::vector<std::string>>();
            if (j.contains("drones")) cfg.drones = j["drones"].get<std::string>();
            if (j.contains("threshold")) j["threshold"].get_to(cfg.threshold);
            if (j.contains("replenish")) j["replenish"].get_to(cfg.replenish);
            if (j.contains("dorm_notstationed_enabled")) j["dorm_notstationed_enabled"].get_to(cfg.dorm_notstationed_enabled);
            if (j.contains("dorm_trust_enabled")) j["dorm_trust_enabled"].get_to(cfg.dorm_trust_enabled);
        }
    };
}

namespace nlohmann {
    // 商店购物配置序列化
    template <>
    struct adl_serializer<ShoppingConfig> {
        static void to_json(json& j, const ShoppingConfig& cfg) {
            j = json{
                {"enable", cfg.enable},
                {"shopping", cfg.shopping},
                {"only_buy_discount", cfg.only_buy_discount},
                {"reserve_max_credit", cfg.reserve_max_credit}
            };
        }
        static void from_json(const json& j, ShoppingConfig& cfg) {
            if (j.contains("enable")) j["enable"].get_to(cfg.enable);
            if (j.contains("shopping")) j["shopping"].get_to(cfg.shopping);
            if (j.contains("only_buy_discount")) j["only_buy_discount"].get_to(cfg.only_buy_discount);
            if (j.contains("reserve_max_credit")) j["reserve_max_credit"].get_to(cfg.reserve_max_credit);
        }
    };
}

// 在JSON序列化/反序列化区域添加
namespace nlohmann {
    // 任务奖励配置序列化
    template <>
    struct adl_serializer<MissionConfig> {
        static void to_json(json& j, const MissionConfig& cfg) {
            j = json{
                {"enable", cfg.enable},
                {"award", cfg.award}
            };
        }
        static void from_json(const json& j, MissionConfig& cfg) {
            if (j.contains("enable")) j["enable"].get_to(cfg.enable);
            if (j.contains("award")) j["award"].get_to(cfg.award);
        }
    };
}

// JSON序列化/反序列化
namespace nlohmann {
    // 奖励配置列表序列化
    template <>
    struct adl_serializer<CollectibleStartList> {
        static void to_json(json& j, const CollectibleStartList& cfg) {
            j = json{
                {"hot_water", cfg.hot_water},
                {"shield", cfg.shield},
                {"ingot", cfg.ingot},
                {"hope", cfg.hope},
                {"random", cfg.random},
                {"key", cfg.key},
                {"dice", cfg.dice},
                {"ideas", cfg.ideas}
            };
        }

        static void from_json(const json& j, CollectibleStartList& cfg) {
            if (j.contains("hot_water")) j["hot_water"].get_to(cfg.hot_water);
            if (j.contains("shield")) j["shield"].get_to(cfg.shield);
            if (j.contains("ingot")) j["ingot"].get_to(cfg.ingot);
            if (j.contains("hope")) j["hope"].get_to(cfg.hope);
            if (j.contains("random")) j["random"].get_to(cfg.random);
            if (j.contains("key")) j["key"].get_to(cfg.key);
            if (j.contains("dice")) j["dice"].get_to(cfg.dice);
            if (j.contains("ideas")) j["ideas"].get_to(cfg.ideas);
        }
    };

    // 肉鸽配置序列化
    template <>
    struct adl_serializer<RoguelikeConfig> {
        static void to_json(json& j, const RoguelikeConfig& cfg) {
            j = json{
                {"enable", cfg.enable},
                {"theme", cfg.theme},
                {"mode", cfg.mode},
                {"squad", cfg.squad},
                {"roles", cfg.roles},
                {"core_char", cfg.core_char},
                {"use_support", cfg.use_support},
                {"use_nonfriend_support", cfg.use_nonfriend_support},
                {"starts_count", cfg.starts_count},
                {"difficulty", cfg.difficulty},
                {"stop_at_final_boss", cfg.stop_at_final_boss},
                {"stop_at_max_level", cfg.stop_at_max_level},
                {"investment_enabled", cfg.investment_enabled},
                {"investments_count", cfg.investments_count},
                {"stop_when_investment_full", cfg.stop_when_investment_full},
                {"investment_with_more_score", cfg.investment_with_more_score},
                {"start_with_elite_two", cfg.start_with_elite_two},
                {"only_start_with_elite_two", cfg.only_start_with_elite_two},
                {"refresh_trader_with_dice", cfg.refresh_trader_with_dice},
                {"collectible_mode_start_list", cfg.collectible_mode_start_list},
                {"use_foldartal", cfg.use_foldartal},
                {"check_collapsal_paradigms", cfg.check_collapsal_paradigms},
                {"double_check_collapsal_paradigms", cfg.double_check_collapsal_paradigms},
                {"monthly_squad_auto_iterate", cfg.monthly_squad_auto_iterate},
                {"monthly_squad_check_comms", cfg.monthly_squad_check_comms},
                {"deep_exploration_auto_iterate", cfg.deep_exploration_auto_iterate},
                {"collectible_mode_shopping", cfg.collectible_mode_shopping},
                {"collectible_mode_squad", cfg.collectible_mode_squad},
                {"collectible_mode_start_list", cfg.collectible_mode_collectibles}
            };
        }

        static void from_json(const json& j, RoguelikeConfig& cfg) {
            // 基础配置
            if (j.contains("enable")) j["enable"].get_to(cfg.enable);

            // 主题（C风格字符串处理）
            if (j.contains("theme")) {
                std::string theme = j["theme"].get<std::string>();
                strncpy(cfg.theme, theme.c_str(), sizeof(cfg.theme) - 1);
                cfg.theme[sizeof(cfg.theme) - 1] = '\0'; // 确保字符串终止
            }

            if (j.contains("mode")) j["mode"].get_to(cfg.mode);

            // 开局分队（C风格字符串处理）
            if (j.contains("squad")) {
                std::string squad = j["squad"].get<std::string>();
                strncpy(cfg.squad, squad.c_str(), sizeof(cfg.squad) - 1);
                cfg.squad[sizeof(cfg.squad) - 1] = '\0';
            }

            // 开局职业组（C风格字符串处理）
            if (j.contains("roles")) {
                std::string roles = j["roles"].get<std::string>();
                strncpy(cfg.roles, roles.c_str(), sizeof(cfg.roles) - 1);
                cfg.roles[sizeof(cfg.roles) - 1] = '\0';
            }

            // 开局干员名（C风格字符串处理）
            if (j.contains("core_char")) {
                std::string core_char = j["core_char"].get<std::string>();
                strncpy(cfg.core_char, core_char.c_str(), sizeof(cfg.core_char) - 1);
                cfg.core_char[sizeof(cfg.core_char) - 1] = '\0';
            }
            if (j.contains("use_support")) j["use_support"].get_to(cfg.use_support);
            if (j.contains("use_nonfriend_support")) j["use_nonfriend_support"].get_to(cfg.use_nonfriend_support);

            // 运行控制
            if (j.contains("starts_count")) j["starts_count"].get_to(cfg.starts_count);
            if (j.contains("difficulty")) j["difficulty"].get_to(cfg.difficulty);
            if (j.contains("stop_at_final_boss")) j["stop_at_final_boss"].get_to(cfg.stop_at_final_boss);
            if (j.contains("stop_at_max_level")) j["stop_at_max_level"].get_to(cfg.stop_at_max_level);

            // 投资相关
            if (j.contains("investment_enabled")) j["investment_enabled"].get_to(cfg.investment_enabled);
            if (j.contains("investments_count")) j["investments_count"].get_to(cfg.investments_count);
            if (j.contains("stop_when_investment_full")) j["stop_when_investment_full"].get_to(cfg.stop_when_investment_full);
            if (j.contains("investment_with_more_score")) j["investment_with_more_score"].get_to(cfg.investment_with_more_score);

            // 凹开局相关
            if (j.contains("start_with_elite_two")) j["start_with_elite_two"].get_to(cfg.start_with_elite_two);
            if (j.contains("only_start_with_elite_two")) j["only_start_with_elite_two"].get_to(cfg.only_start_with_elite_two);
            if (j.contains("collectible_mode_start_list")) cfg.collectible_mode_start_list = j["collectible_mode_start_list"].get<CollectibleStartList>();

            // 主题专属配置
            if (j.contains("refresh_trader_with_dice")) j["refresh_trader_with_dice"].get_to(cfg.refresh_trader_with_dice);
            if (j.contains("use_foldartal")) j["use_foldartal"].get_to(cfg.use_foldartal);
            if (j.contains("check_collapsal_paradigms")) j["check_collapsal_paradigms"].get_to(cfg.check_collapsal_paradigms);
            if (j.contains("double_check_collapsal_paradigms")) j["double_check_collapsal_paradigms"].get_to(cfg.double_check_collapsal_paradigms);

            // 自动切换设置
            if (j.contains("monthly_squad_auto_iterate")) j["monthly_squad_auto_iterate"].get_to(cfg.monthly_squad_auto_iterate);
            if (j.contains("monthly_squad_check_comms")) j["monthly_squad_check_comms"].get_to(cfg.monthly_squad_check_comms);
            if (j.contains("deep_exploration_auto_iterate")) j["deep_exploration_auto_iterate"].get_to(cfg.deep_exploration_auto_iterate);

            // 烧水相关配置
            if (j.contains("collectible_mode_shopping")) j["collectible_mode_shopping"].get_to(cfg.collectible_mode_shopping);
            if (j.contains("collectible_mode_squad")) {
                std::string squad = j["collectible_mode_squad"].get<std::string>();
                strncpy(cfg.collectible_mode_squad, squad.c_str(), sizeof(cfg.collectible_mode_squad) - 1);
                cfg.collectible_mode_squad[sizeof(cfg.collectible_mode_squad) - 1] = '\0';
            }
            if (j.contains("collectible_mode_start_list")) cfg.collectible_mode_collectibles = j["collectible_mode_start_list"].get<CollectibleStartList>();
        }
    };
}

class SettingsManager {
private:
    StartupConfig startup_config;
    StageConfig stage_config;
    RecruitmentConfig recruitment_config;
    FacilityConfig facility_config;  // 新增设施配置
    ShoppingConfig shopping_config;  // 商店购物配置
    MissionConfig mission_config;  // 任务奖励配置
    RoguelikeConfig roguelike_config;
    std::string config_path;

    // 窗口状态
    bool settings_window_open = false;
    bool startup_settings_open = false;
    bool stage_settings_open = false;
    bool recruitment_settings_open = false;
    bool facility_settings_open = false;  // 设施配置窗口
    bool stage_picker_open = false;
    bool drops_editor_open = false;
    bool tags_editor_open = false;
    bool facility_editor_open = false;  // 设施编辑窗口
    bool shopping_settings_open = false;  // 购物配置窗口
    bool mission_settings_open = false;  // 任务配置窗口
    bool roguelike_settings_open = false;

    // 自定义窗口临时缓冲区
    char temp_stage[STAGE_BUFFER_SIZE] = { 0 };
    std::map<std::string, int> temp_drops;
    char new_tag[TAG_BUFFER_SIZE] = { 0 };
    char new_facility[FACILITY_BUFFER_SIZE] = { 0 };  // 新设施临时输入
    std::vector<std::string> temp_facilities;  // 设施编辑临时存储

    // UI尺寸常量
    const float text_width = 120.0f;
    const float input_max_width = 180.0f;
    const float window_min_width = 400.0f;

    // 设施选项
    const std::vector<const char*> facility_options = {
        "Mfg", "Trade", "Power", "Control",
        "Reception", "Office", "Dorm"
    };

    // 无人机用途选项
    const std::vector<std::pair<const char*, const char*>> drones_options = {
        {"_NotUse", "不使用无人机"},
        {"Money", "贸易站-龙门币"},
        {"SyntheticJade", "贸易站-源石碎片"},
        {"CombatRecord", "制造站-作战记录"},
        {"PureGold", "制造站-赤金"},
        {"OriginStone", "制造站-源石碎片"},
        {"Chip", "制造站-芯片"}
    };

    const std::vector<const char*> client_types = {
        "Official", "Bilibili", "txwy",
        "YoStarEN", "YoStarJP", "YoStarKR"
    };
    const std::vector<std::pair<int, const char*>> series_options = {
        {-1, "禁用"}, {0, "自动"},
        {1, "1次"}, {2, "2次"}, {3, "3次"},
        {4, "4次"}, {5, "5次"}, {6, "6次"}
    };
    const std::vector<std::pair<int, const char*>> extra_tags_options = {
        {0, "默认行为"},
        {1, "选3个Tags(可能冲突)"},
        {2, "更多高星组合(可能冲突)"}
    };
    const std::vector<const char*> server_options = {
        "CN", "US", "JP", "KR"
    };


    // 主题选项（ID -> 显示名称）
    const std::vector<std::pair<std::string, std::string>> theme_options = {
        {"Phantom", "傀影与猩红血钻"},
        {"Mizuki", "水月与深蓝之树"},
        {"Sami", "探索者的银凇止境"},
        {"Sarkaz", "萨卡兹的无终奇语"},
        {"JieGarden", "界园"}
    };

    // 模式选项（值 -> 描述）
    const std::vector<std::pair<int, std::string>> mode_options = {
        {0, "刷分/奖励点数（稳定打更多层数）"},
        {1, "刷源石锭（第一层投资完退出）"},
        {4, "凹开局（特定流程）"},
        {6, "刷月度小队蚊子腿"},
        {7, "刷深入调查蚊子腿"}
    };

    // 根据主题获取可用分队列表
    std::vector<std::string> get_squad_options(const std::string& theme) {
        if (theme == "Phantom") {
            return { "指挥分队", "集群分队", "后勤分队", "矛头分队",
                    "突击战术分队", "堡垒战术分队", "远程战术分队",
                    "破坏战术分队", "研究分队", "高规格分队" };
        }
        else if (theme == "Mizuki") {
            return { "心胜于物分队", "物尽其用分队", "以人为本分队", "指挥分队",
                    "集群分队", "后勤分队", "矛头分队", "突击战术分队",
                    "堡垒战术分队", "远程战术分队", "破坏战术分队",
                    "研究分队", "高规格分队" };
        }
        else if (theme == "Sami") {
            return { "永恒狩猎分队", "生活至上分队", "科学主义分队", "特训分队",
                    "指挥分队", "集群分队", "后勤分队", "矛头分队",
                    "突击战术分队", "堡垒战术分队", "远程战术分队",
                    "破坏战术分队", "高规格分队" };
        }
        else if (theme == "Sarkaz") {
            return { "因地制宜分队", "魂灵护送分队", "博闻广记分队", "蓝图测绘分队",
                    "指挥分队", "集群分队", "后勤分队", "矛头分队",
                    "突击战术分队", "堡垒战术分队", "远程战术分队",
                    "破坏战术分队", "高规格分队", "点刺成锭分队",
                    "拟态学者分队", "异想天开分队" };
        }
        else if (theme == "JieGarden") {
            return { "特勤分队", "高台突破分队", "地面突破分队", "游客分队",
                    "司岁台分队", "天师府分队", "指挥分队", "后勤分队",
                    "突击战术分队", "堡垒战术分队", "远程战术分队",
                    "破坏战术分队", "高规格分队" };
        }
        return {};
    }

    // 根据主题获取可用职业组列表
    std::vector<std::string> get_roles_options(const std::string& theme) {
        if (theme == "JieGarden") {
            return { "先手必胜", "稳扎稳打", "取长补短", "灵活部署",
                    "坚不可摧", "随心所欲" };
        }
        else {
            return { "先手必胜", "稳扎稳打", "取长补短", "随心所欲" };
        }
    }

    // 3. 修复主题切换时的同步逻辑
    void on_theme_changed(const std::string& old_theme, const std::string& new_theme) {
        if (old_theme == new_theme) return;

        // 重置职业组为"先手必胜"
        strncpy(roguelike_config.roles, "先手必胜", sizeof(roguelike_config.roles) - 1);
        roguelike_config.roles[sizeof(roguelike_config.roles) - 1] = '\0';

        // 重置分队为列表第一个
        auto squads = get_squad_options(new_theme);
        if (!squads.empty()) {
            // 同步更新开局分队和烧水分队
            strncpy(roguelike_config.squad, squads[0].c_str(), sizeof(roguelike_config.squad) - 1);
            roguelike_config.squad[sizeof(roguelike_config.squad) - 1] = '\0';

            // 保持烧水分队与开局分队同步
            strncpy(roguelike_config.collectible_mode_squad, squads[0].c_str(),
                sizeof(roguelike_config.collectible_mode_squad) - 1);
            roguelike_config.collectible_mode_squad[sizeof(roguelike_config.collectible_mode_squad) - 1] = '\0';
        }
    }

    void sync_server_with_client_type() {
        // 同步关卡配置服务器
        strncpy(stage_config.client_type, startup_config.client_type, STR_BUFFER_SIZE - 1);
        // 同步公招配置服务器
        strncpy(recruitment_config.client_type, startup_config.client_type, STR_BUFFER_SIZE - 1);

        // 根据客户端类型设置服务器
        if (strcmp(startup_config.client_type, "Official") == 0 ||
            strcmp(startup_config.client_type, "Bilibili") == 0) {
            strncpy(stage_config.server, "CN", STR_BUFFER_SIZE - 1);
            strncpy(recruitment_config.server, "CN", STR_BUFFER_SIZE - 1);
        }
        else if (strcmp(startup_config.client_type, "txwy") == 0) {
            strncpy(stage_config.server, "TW", STR_BUFFER_SIZE - 1);
            strncpy(recruitment_config.server, "TW", STR_BUFFER_SIZE - 1);
        }
        else if (strcmp(startup_config.client_type, "YoStarEN") == 0) {
            strncpy(stage_config.server, "US", STR_BUFFER_SIZE - 1);
            strncpy(recruitment_config.server, "US", STR_BUFFER_SIZE - 1);
        }
        else if (strcmp(startup_config.client_type, "YoStarJP") == 0) {
            strncpy(stage_config.server, "JP", STR_BUFFER_SIZE - 1);
            strncpy(recruitment_config.server, "JP", STR_BUFFER_SIZE - 1);
        }
        else if (strcmp(startup_config.client_type, "YoStarKR") == 0) {
            strncpy(stage_config.server, "KR", STR_BUFFER_SIZE - 1);
            strncpy(recruitment_config.server, "KR", STR_BUFFER_SIZE - 1);
        }
    }

    bool can_set_account() const {
        return strcmp(startup_config.client_type, "Official") == 0 ||
            strcmp(startup_config.client_type, "Bilibili") == 0;
    }

    void draw_label(const char* text) {
        ImGui::Text("%s", text);
        ImGui::SameLine(text_width);
    }

    void draw_stage_picker() {
        if (!stage_picker_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 200), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 250), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("选择关卡", &stage_picker_open)) {
            ImGui::Text("请选择要刷取的关卡...");
            ImGui::Spacing();

            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputText("##stage_preview", temp_stage, STAGE_BUFFER_SIZE);

            if (ImGui::Button("确认选择##stage_picker")) {
                strncpy(stage_config.stage, temp_stage, STAGE_BUFFER_SIZE - 1);
                stage_picker_open = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##stage_picker")) {
                stage_picker_open = false;
            }
        }
        ImGui::End();
    }

    void draw_drops_editor() {
        if (!drops_editor_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 200), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("编辑掉落条件", &drops_editor_open)) {
            ImGui::Text("请设置需要的掉落材料...");
            ImGui::Spacing();

            static bool initialized = false;
            if (!initialized) {
                temp_drops = stage_config.drops;
                initialized = true;
            }

            static char new_item_id[32] = { 0 };
            static int new_item_count = 1;

            if (ImGui::BeginTable("drops_temp", 2, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("材料ID", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("数量", ImGuiTableColumnFlags_WidthFixed, 60);
                ImGui::TableHeadersRow();

                for (auto& [id, count] : temp_drops) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%s", id.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::InputInt(("##count_" + id).c_str(), &count);
                }
                ImGui::EndTable();
            }

            ImGui::SetNextItemWidth(80);
            ImGui::InputText("##new_item_id", new_item_id, 32);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::InputInt("##new_item_count", &new_item_count);
            ImGui::SameLine();
            if (ImGui::Button("添加##drops")) {
                if (strlen(new_item_id) > 0 && new_item_count > 0) {
                    temp_drops[new_item_id] = new_item_count;
                }
            }

            if (ImGui::Button("确认设置##drops")) {
                stage_config.drops = temp_drops;
                initialized = false;
                drops_editor_open = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##drops")) {
                initialized = false;
                drops_editor_open = false;
            }
        }
        ImGui::End();
    }

    void draw_tags_editor() {
        if (!tags_editor_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 200), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("编辑首选标签", &tags_editor_open)) {
            ImGui::Text("仅在Tag等级为3时有效");
            ImGui::Spacing();

            // 显示当前标签
            if (ImGui::BeginListBox("##current_tags", ImVec2(-1, 150))) {
                for (size_t i = 0; i < recruitment_config.first_tags.size(); ++i) {
                    const char* tag = recruitment_config.first_tags[i].c_str();
                    char label[256];
                    sprintf(label, "%s##tag_%d", tag, (int)i);
                    if (ImGui::Selectable(label)) {
                        // 点击删除
                        recruitment_config.first_tags.erase(recruitment_config.first_tags.begin() + i);
                        break;
                    }
                }
                ImGui::EndListBox();
            }

            // 添加新标签
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputText("##new_tag", new_tag, TAG_BUFFER_SIZE);
            ImGui::SameLine();
            if (ImGui::Button("添加标签")) {
                if (strlen(new_tag) > 0) {
                    recruitment_config.first_tags.push_back(new_tag);
                    memset(new_tag, 0, TAG_BUFFER_SIZE);
                }
            }

            if (ImGui::Button("确认##tags")) {
                tags_editor_open = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##tags")) {
                tags_editor_open = false;
            }
        }
        ImGui::End();
    }

    // 新增：设施编辑窗口
    void draw_facility_editor() {
        if (!facility_editor_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 250), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 350), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("编辑换班设施", &facility_editor_open)) {
            ImGui::Text("设置要换班的设施（有序）");
            ImGui::TextDisabled("不支持运行中修改");
            ImGui::Spacing();

            static bool initialized = false;
            static size_t selected_index = SIZE_MAX; // 用于跟踪选中的项
            if (!initialized) {
                temp_facilities = facility_config.facility;
                selected_index = SIZE_MAX;
                memset(new_facility, 0, FACILITY_BUFFER_SIZE);
                initialized = true;
            }

            // 显示当前设施列表 - 使用Table布局
            if (ImGui::BeginTable("facilities_table", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_NoSavedSettings)) {
                // 表格列设置
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 20); // 选择框列
                ImGui::TableSetupColumn("设施名称", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("排序", ImGuiTableColumnFlags_WidthFixed, 100);
                ImGui::TableHeadersRow();

                // 遍历设施列表
                for (size_t i = 0; i < temp_facilities.size(); ++i) {
                    ImGui::TableNextRow();

                    // 选择框列 - 单独的选择区域，避免与删除冲突
                    ImGui::TableSetColumnIndex(0);
                    char check_label[32];
                    sprintf(check_label, "##check_%d", (int)i);
                    bool is_selected = (selected_index == i);  // 使用临时变量存储状态
                    if (ImGui::Checkbox(check_label, &is_selected)) {
                        // 根据复选框状态更新选中索引
                        selected_index = is_selected ? i : SIZE_MAX;
                    }

                    // 设施名称列
                    ImGui::TableSetColumnIndex(1);
                    char label[256];
                    sprintf(label, "%s##name_%d", temp_facilities[i].c_str(), (int)i);
                    ImGui::TextUnformatted(temp_facilities[i].c_str());

                    // 排序和删除按钮列
                    ImGui::TableSetColumnIndex(2);
                    ImGui::PushID((int)i);

                    // 上移按钮
                    bool can_move_up = i > 0;
                    if (!can_move_up) ImGui::BeginDisabled();
                    if (ImGui::Button("↑", ImVec2(25, 20))) {
                        std::swap(temp_facilities[i], temp_facilities[i - 1]);
                        // 交换后保持选中状态
                        if (selected_index == i) selected_index = i - 1;
                        else if (selected_index == i - 1) selected_index = i;
                    }
                    if (!can_move_up) ImGui::EndDisabled();

                    ImGui::SameLine();

                    // 下移按钮
                    bool can_move_down = i < temp_facilities.size() - 1;
                    if (!can_move_down) ImGui::BeginDisabled();
                    if (ImGui::Button("↓", ImVec2(25, 20))) {
                        std::swap(temp_facilities[i], temp_facilities[i + 1]);
                        // 交换后保持选中状态
                        if (selected_index == i) selected_index = i + 1;
                        else if (selected_index == i + 1) selected_index = i;
                    }
                    if (!can_move_down) ImGui::EndDisabled();

                    ImGui::SameLine();

                    // 删除按钮 - 单独的删除按钮，避免与选择冲突
                    if (ImGui::Button("×", ImVec2(25, 20))) {
                        temp_facilities.erase(temp_facilities.begin() + i);
                        if (selected_index == i) selected_index = SIZE_MAX;
                        else if (selected_index > i) selected_index--;
                        break; // 因为删除了元素，需要重新遍历
                    }

                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            ImGui::Spacing();

            // 添加新设施
            ImGui::SetNextItemWidth(input_max_width);
            const char* combo_label = "选择设施...";
            if (strlen(new_facility) > 0) {
                combo_label = new_facility;
            }

            if (ImGui::BeginCombo("##new_facility", combo_label)) {
                for (const char* facility : facility_options) {
                    bool is_selected = (strcmp(new_facility, facility) == 0);
                    if (ImGui::Selectable(facility, is_selected)) {
                        strncpy(new_facility, facility, FACILITY_BUFFER_SIZE - 1);
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("添加设施")) {
                if (strlen(new_facility) > 0) {
                    bool exists = false;
                    for (const auto& f : temp_facilities) {
                        if (f == new_facility) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        temp_facilities.push_back(new_facility);
                    }
                    memset(new_facility, 0, FACILITY_BUFFER_SIZE);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("确认设置##facility")) {
                facility_config.facility = temp_facilities;
                initialized = false;
                facility_editor_open = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##facility")) {
                initialized = false;
                facility_editor_open = false;
            }
        }
        ImGui::End();
    }


    bool is_string_in_list(const char* str, const std::vector<std::string>& list) {
        if (!str) return false;
        return std::find(list.begin(), list.end(), std::string(str)) != list.end();
    }

public:
    SettingsManager(std::string path) : config_path(std::move(path)) {
        load_config();
        sync_server_with_client_type();
    }

    void load_config() {
        try {
            std::ifstream file(config_path);
            if (file.is_open()) {
                json j;
                file >> j;
                if (j.contains("startup")) startup_config = j["startup"].get<StartupConfig>();
                if (j.contains("stage")) stage_config = j["stage"].get<StageConfig>();
                if (j.contains("recruitment")) recruitment_config = j["recruitment"].get<RecruitmentConfig>();
                if (j.contains("facility")) facility_config = j["facility"].get<FacilityConfig>();  // 加载设施配置
                if (j.contains("shopping")) shopping_config = j["shopping"].get<ShoppingConfig>();
                if (j.contains("mission")) mission_config = j["mission"].get<MissionConfig>();
                if (j.contains("roguelike")) roguelike_config = j["roguelike"].get<RoguelikeConfig>();
            }
        }
        catch (const std::exception& e) {
            // 初始化默认配置
            memset(&startup_config, 0, sizeof(StartupConfig));
            startup_config.enable = true;
            startup_config.start_game_enabled = true;

            memset(&stage_config, 0, sizeof(StageConfig));
            stage_config.enable = true;
            stage_config.times = 255;
            stage_config.series = -1;
            stage_config.report_to_penguin = false;
            strncpy(stage_config.server, "CN", STR_BUFFER_SIZE - 1);

            // 初始化公招默认配置
            memset(&recruitment_config, 0, sizeof(RecruitmentConfig));
            recruitment_config.enable = true;
            recruitment_config.refresh = true;
            recruitment_config.select = { 3, 4 };
            recruitment_config.confirm = { 3, 4 };
            recruitment_config.extra_tags_mode = 0;
            recruitment_config.times = 3;
            recruitment_config.set_time = true;
            recruitment_config.expedite = false;
            recruitment_config.expedite_times = 0;
            recruitment_config.skip_robot = true;
            recruitment_config.recruitment_time = {};
            recruitment_config.report_to_penguin = false;
            recruitment_config.report_to_yituliu = false;
            strncpy(recruitment_config.server, "CN", STR_BUFFER_SIZE - 1);

            // 初始化设施默认配置
            memset(&facility_config, 0, sizeof(FacilityConfig));
            facility_config.enable = true;
            facility_config.mode = 0;
            // 设置默认设施顺序：Mfg -> Trade -> Power -> Control -> Reception -> Office -> Dorm
            facility_config.facility = { "Mfg", "Trade", "Power", "Control", "Reception", "Office", "Dorm" };
            facility_config.drones = "_NotUse";
            facility_config.threshold = 0.3f;
            facility_config.replenish = false;
            facility_config.dorm_notstationed_enabled = false;
            facility_config.dorm_trust_enabled = false;
            temp_facilities = facility_config.facility;


            // 初始化商店购物默认配置
            memset(&shopping_config, 0, sizeof(ShoppingConfig));
            shopping_config.enable = true;
            shopping_config.shopping = true;
            shopping_config.only_buy_discount = false;
            shopping_config.reserve_max_credit = false;

            memset(&mission_config, 0, sizeof(MissionConfig));
            mission_config.enable = true;
            mission_config.award = true;

            roguelike_config = RoguelikeConfig(); // 使用结构体默认值
        }
    }

    void save_config() {
        try {
            std::ofstream file(config_path);
            if (file.is_open()) {
                json j;
                j["startup"] = startup_config;
                j["stage"] = stage_config;
                j["recruitment"] = recruitment_config;
                j["facility"] = facility_config;  // 保存设施配置
                j["shopping"] = shopping_config;  // 保存购物配置
                j["mission"] = mission_config;  // 保存任务配置
                j["roguelike"] = roguelike_config;
                file << std::setw(4) << j << std::endl;
            }
        }
        catch (const std::exception& e) {
            // 错误处理
        }
    }

    void draw_settings_button() {
        //if (ImGui::Button("打开设置##main_button")) {
        settings_window_open = true;
        //}
    }

    void draw_startup_subpage() {
        // 保持原有实现不变
        if (!startup_settings_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 250), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 280), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("启动参数设置", &startup_settings_open)) {
            draw_label("是否启用本任务");
            ImGui::Checkbox("##startup_enable", &startup_config.enable);
            ImGui::Spacing();

            draw_label("客户端版本");
            ImGui::SetNextItemWidth(input_max_width);
            if (ImGui::BeginCombo("##startup_client", startup_config.client_type)) {
                for (const char* type : client_types) {
                    bool selected = (strcmp(startup_config.client_type, type) == 0);
                    if (ImGui::Selectable(type, selected)) {
                        strncpy(startup_config.client_type, type, STR_BUFFER_SIZE - 1);
                        sync_server_with_client_type();
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            draw_label("自动启动客户端");
            ImGui::Text("已启用");
            ImGui::Spacing();

            draw_label("切换账号");
            bool is_enabled = can_set_account();
            if (!is_enabled) ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputText("##account_name", startup_config.account_name, ACCOUNT_BUFFER_SIZE);
            if (!is_enabled) {
                ImGui::EndDisabled();
                ImGui::TextDisabled("仅Official和Bilibili支持");
            }
            else {
                ImGui::TextDisabled("输入账号唯一标识");
            }
            ImGui::Spacing();

            if (ImGui::Button("保存##startup")) {
                save_config();
                ImGui::OpenPopup("提示##startup");
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##startup")) {
                startup_settings_open = false;
            }

            if (ImGui::BeginPopup("提示##startup")) {
                ImGui::Text("设置已保存");
                if (ImGui::Button("确定##startup")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    void draw_stage_subpage() {
        // 保持原有实现不变
        if (!stage_settings_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 400), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 450), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("关卡任务设置", &stage_settings_open)) {
            draw_label("是否启用本任务");
            ImGui::Checkbox("##stage_enable", &stage_config.enable);
            ImGui::Spacing();

            draw_label("关卡名");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputText("##stage_name", stage_config.stage, STAGE_BUFFER_SIZE);
            ImGui::SameLine();
            if (ImGui::Button("选择##stage_picker_btn")) {
                strncpy(temp_stage, stage_config.stage, STAGE_BUFFER_SIZE - 1);
                stage_picker_open = true;
            }
            ImGui::TextDisabled("格式：1-7、S3-2Hard等");
            ImGui::Spacing();

            draw_label("理智药数量");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputInt("##medicine_count", &stage_config.medicine);
            ImGui::Spacing();

            draw_label("过期理智药数量");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputInt("##exp_medicine_count", &stage_config.expiring_medicine);
            ImGui::Spacing();

            draw_label("石头数量");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputInt("##stone_count", &stage_config.stone);
            ImGui::Spacing();

            draw_label("战斗次数");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputInt("##battle_times", &stage_config.times);
            ImGui::Spacing();

            draw_label("连战次数");
            ImGui::SetNextItemWidth(input_max_width);
            const char* current_series = "未知";
            for (const auto& [val, str] : series_options) {
                if (val == stage_config.series) {
                    current_series = str;
                    break;
                }
            }
            if (ImGui::BeginCombo("##series_combo", current_series)) {
                for (const auto& [val, str] : series_options) {
                    bool selected = (stage_config.series == val);
                    if (ImGui::Selectable(str, selected)) {
                        stage_config.series = val;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            draw_label("指定掉落");
            if (ImGui::BeginTable("drops_table", 2, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("材料ID", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("数量", ImGuiTableColumnFlags_WidthFixed, 60);
                ImGui::TableHeadersRow();

                if (stage_config.drops.empty()) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("无数据");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("--");
                }
                else {
                    for (const auto& [id, count] : stage_config.drops) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", id.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%d", count);
                    }
                }
                ImGui::EndTable();
            }
            ImGui::SameLine();
            if (ImGui::Button("选择材料##drops_btn")) {
                drops_editor_open = true;
            }
            ImGui::Spacing();

            draw_label("汇报企鹅数据");
            ImGui::Checkbox("##report_to_penguin", &stage_config.report_to_penguin);
            ImGui::Spacing();

            draw_label("企鹅ID");
            if (!stage_config.report_to_penguin) ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputText("##penguin_id_input", stage_config.penguin_id, PENGUIN_ID_BUFFER_SIZE);
            if (!stage_config.report_to_penguin) ImGui::EndDisabled();
            ImGui::Spacing();

            draw_label("服务器");
            ImGui::Text("%s", stage_config.server);
            ImGui::Spacing();

            draw_label("客户端版本");
            ImGui::Text("%s", stage_config.client_type);
            ImGui::Spacing();

            draw_label("节省碎石模式");
            ImGui::Checkbox("##dr_grandet_mode", &stage_config.DrGrandet);
            ImGui::TextDisabled("等待理智恢复后碎石");
            ImGui::Spacing();

            if (ImGui::Button("保存##stage")) {
                save_config();
                ImGui::OpenPopup("提示##stage");
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##stage")) {
                stage_settings_open = false;
            }

            if (ImGui::BeginPopup("提示##stage")) {
                ImGui::Text("设置已保存");
                if (ImGui::Button("确定##stage")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();

        draw_stage_picker();
        draw_drops_editor();
    }

    void draw_recruitment_subpage() {
        // 保持原有实现不变
        if (!recruitment_settings_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 450), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("公招配置", &recruitment_settings_open)) {
            draw_label("是否启用本任务");
            ImGui::Checkbox("##recruitment_enable", &recruitment_config.enable);
            ImGui::Spacing();

            draw_label("选择标签等级");
            ImGui::Text("3, 4");
            ImGui::Spacing();

            draw_label("确认标签等级");
            ImGui::Text("3, 4");
            ImGui::Spacing();

            draw_label("首选标签");
            if (recruitment_config.first_tags.empty()) {
                ImGui::TextDisabled("无标签");
            }
            else {
                ImGui::Text("%zu个标签", recruitment_config.first_tags.size());
            }
            ImGui::SameLine();
            if (ImGui::Button("编辑##tags_btn")) {
                memset(new_tag, 0, TAG_BUFFER_SIZE);
                tags_editor_open = true;
            }
            ImGui::TextDisabled("仅Tag等级为3时有效");
            ImGui::Spacing();

            draw_label("额外标签模式");
            ImGui::SetNextItemWidth(input_max_width);
            const char* current_mode = "未知";
            for (const auto& [val, str] : extra_tags_options) {
                if (val == recruitment_config.extra_tags_mode) {
                    current_mode = str;
                    break;
                }
            }
            if (ImGui::BeginCombo("##extra_tags_mode", current_mode)) {
                for (const auto& [val, str] : extra_tags_options) {
                    bool selected = (recruitment_config.extra_tags_mode == val);
                    if (ImGui::Selectable(str, selected)) {
                        recruitment_config.extra_tags_mode = val;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            draw_label("招募次数");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputInt("##recruitment_times", &recruitment_config.times);
            ImGui::Spacing();

            draw_label("使用加急许可");
            ImGui::Checkbox("##expedite", &recruitment_config.expedite);
            ImGui::Spacing();

            draw_label("加急次数");
            if (!recruitment_config.expedite) ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputInt("##expedite_times", &recruitment_config.expedite_times);
            if (!recruitment_config.expedite) ImGui::EndDisabled();
            ImGui::TextDisabled("0表示无限制");
            ImGui::Spacing();

            draw_label("跳过小车词条");
            ImGui::Checkbox("##skip_robot", &recruitment_config.skip_robot);
            ImGui::Spacing();

            draw_label("汇报企鹅数据");
            ImGui::Checkbox("##recruit_penguin", &recruitment_config.report_to_penguin);
            ImGui::Spacing();

            draw_label("企鹅ID");
            if (!recruitment_config.report_to_penguin) ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputText("##recruit_penguin_id", recruitment_config.penguin_id, PENGUIN_ID_BUFFER_SIZE);
            if (!recruitment_config.report_to_penguin) ImGui::EndDisabled();
            ImGui::Spacing();

            draw_label("汇报一图流数据");
            ImGui::Checkbox("##report_to_yituliu", &recruitment_config.report_to_yituliu);
            ImGui::Spacing();

            draw_label("一图流ID");
            if (!recruitment_config.report_to_yituliu) ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputText("##yituliu_id", recruitment_config.yituliu_id, YITULIU_ID_BUFFER_SIZE);
            if (!recruitment_config.report_to_yituliu) ImGui::EndDisabled();
            ImGui::Spacing();

            draw_label("服务器");
            ImGui::SetNextItemWidth(input_max_width);
            if (ImGui::BeginCombo("##recruit_server", recruitment_config.server)) {
                for (const char* server : server_options) {
                    bool selected = (strcmp(recruitment_config.server, server) == 0);
                    if (ImGui::Selectable(server, selected)) {
                        strncpy(recruitment_config.server, server, STR_BUFFER_SIZE - 1);
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            if (ImGui::Button("保存##recruitment")) {
                save_config();
                ImGui::OpenPopup("提示##recruitment");
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##recruitment")) {
                recruitment_settings_open = false;
            }

            if (ImGui::BeginPopup("提示##recruitment")) {
                ImGui::Text("设置已保存");
                if (ImGui::Button("确定##recruitment")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();

        draw_tags_editor();
    }

    // 新增：设施配置子页面
    void draw_facility_subpage() {
        if (!facility_settings_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 400), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 450), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("设施换班配置", &facility_settings_open)) {
            // 启用本任务
            draw_label("是否启用本任务");
            ImGui::Checkbox("##facility_enable", &facility_config.enable);
            ImGui::Spacing();

            // 模式选择
            draw_label("模式");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputInt("##facility_mode", &facility_config.mode);
            ImGui::Spacing();

            // 换班设施列表
            draw_label("换班设施");
            if (facility_config.facility.empty()) {
                ImGui::TextDisabled("无设施");
            }
            else {
                ImGui::Text("%zu个设施", facility_config.facility.size());
            }
            ImGui::SameLine();
            if (ImGui::Button("编辑##facility_btn")) {
                memset(new_facility, 0, FACILITY_BUFFER_SIZE);
                facility_editor_open = true;
            }
            ImGui::TextDisabled("不支持运行中修改");
            ImGui::Spacing();

            // 无人机用途
            draw_label("无人机用途");
            ImGui::SetNextItemWidth(input_max_width);
            const char* current_drone = "未知";
            for (const auto& [val, desc] : drones_options) {
                if (facility_config.drones == val) {
                    current_drone = desc;
                    break;
                }
            }
            if (ImGui::BeginCombo("##drones_usage", current_drone)) {
                for (const auto& [val, desc] : drones_options) {
                    bool selected = (facility_config.drones == val);
                    if (ImGui::Selectable(desc, selected)) {
                        facility_config.drones = val;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            // 工作心情阈值
            draw_label("心情阈值");
            ImGui::SetNextItemWidth(input_max_width);
            ImGui::InputFloat("##mood_threshold", &facility_config.threshold, 0.1f, 0.0f, "%.1f");
            ImGui::TextDisabled("范围 [0, 1.0]，默认 0.3");
            ImGui::Spacing();

            // 贸易站自动补货
            draw_label("自动补货");
            ImGui::Checkbox("##auto_replenish", &facility_config.replenish);
            ImGui::TextDisabled("贸易站“源石碎片”自动补货");
            ImGui::Spacing();

            // 宿舍“未进驻”选项
            draw_label("未进驻选项");
            ImGui::Checkbox("##dorm_notstationed", &facility_config.dorm_notstationed_enabled);
            ImGui::TextDisabled("启用宿舍“未进驻”选项");
            ImGui::Spacing();

            // 宿舍信赖未满选项
            draw_label("信赖未满选项");
            ImGui::Checkbox("##dorm_trust", &facility_config.dorm_trust_enabled);
            ImGui::TextDisabled("宿舍填入信赖未满干员");
            ImGui::Spacing();

            // 保存按钮
            if (ImGui::Button("保存##facility")) {
                save_config();
                ImGui::OpenPopup("提示##facility");
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##facility")) {
                facility_settings_open = false;
            }

            if (ImGui::BeginPopup("提示##facility")) {
                ImGui::Text("设置已保存");
                if (ImGui::Button("确定##facility")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();

        draw_facility_editor();  // 绘制设施编辑窗口
    }

    // 新增：购物配置子页面
    void draw_shopping_subpage() {
        if (!shopping_settings_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 250), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("商店购物配置", &shopping_settings_open)) {
            // 启用本任务
            draw_label("是否启用本任务");
            ImGui::Checkbox("##shopping_enable", &shopping_config.enable);
            ImGui::Spacing();

            // 是否购物
            draw_label("是否购物");
            ImGui::Checkbox("##do_shopping", &shopping_config.shopping);
            ImGui::Spacing();

            // 是否只购买折扣物品
            draw_label("只买折扣物品");
            ImGui::Checkbox("##only_discount", &shopping_config.only_buy_discount);
            ImGui::TextDisabled("仅作用于第二轮购买");
            ImGui::Spacing();

            // 信用点低于300时停止购买
            draw_label("保留信用点");
            ImGui::Checkbox("##reserve_credit", &shopping_config.reserve_max_credit);
            ImGui::TextDisabled("信用点低于300时停止购买，仅作用于第二轮");
            ImGui::Spacing();

            // 保存按钮
            if (ImGui::Button("保存##shopping")) {
                save_config();
                ImGui::OpenPopup("提示##shopping");
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##shopping")) {
                shopping_settings_open = false;
            }

            if (ImGui::BeginPopup("提示##shopping")) {
                ImGui::Text("设置已保存");
                if (ImGui::Button("确定##shopping")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }


    // 新增：任务配置子页面
    void draw_mission_subpage() {
        if (!mission_settings_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 200), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 220), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("任务奖励配置", &mission_settings_open)) {
            // 启用本任务
            draw_label("是否启用本任务");
            ImGui::Checkbox("##mission_enable", &mission_config.enable);
            ImGui::Spacing();

            // 领取每日/每周任务奖励
            draw_label("领取任务奖励");
            ImGui::Checkbox("##get_award", &mission_config.award);
            ImGui::TextDisabled("包括每日任务和每周任务奖励");
            ImGui::Spacing();

            // 保存按钮
            if (ImGui::Button("保存##mission")) {
                save_config();
                ImGui::OpenPopup("提示##mission");
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##mission")) {
                mission_settings_open = false;
            }

            if (ImGui::BeginPopup("提示##mission")) {
                ImGui::Text("设置已保存");
                if (ImGui::Button("确定##mission")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    // 绘制奖励配置编辑器（带主题条件控制）
    void draw_collectible_editor(CollectibleStartList& list, const std::string& theme) {
        ImGui::Text("期望奖励配置:");
        ImGui::Spacing();

        // 通用奖励
        ImGui::Checkbox("热水壶奖励##hw", &list.hot_water);
        ImGui::Checkbox("护盾奖励##sh", &list.shield);
        ImGui::Checkbox("源石锭奖励##ig", &list.ingot);

        // 希望奖励（JieGarden不可用）
        if (theme != "JieGarden") {
            ImGui::Checkbox("希望奖励##hp", &list.hope);
        }
        else {
            if (list.hope) list.hope = false; // 强制重置
            ImGui::BeginDisabled();
            ImGui::Checkbox("希望奖励（当前主题不可用）##hp_dis", &list.hope);
            ImGui::EndDisabled();
        }

        ImGui::Checkbox("随机奖励##rd", &list.random);

        // Mizuki专属奖励
        if (theme == "Mizuki") {
            ImGui::Checkbox("钥匙奖励##ky", &list.key);
            ImGui::Checkbox("骰子奖励##dc", &list.dice);
        }
        else {
            if (list.key) list.key = false;
            if (list.dice) list.dice = false;
            ImGui::BeginDisabled();
            ImGui::Checkbox("钥匙奖励（仅Mizuki可用）##ky_dis", &list.key);
            ImGui::Checkbox("骰子奖励（仅Mizuki可用）##dc_dis", &list.dice);
            ImGui::EndDisabled();
        }

        // Sarkaz专属奖励
        if (theme == "Sarkaz") {
            ImGui::Checkbox("2构想奖励##id", &list.ideas);
        }
        else {
            if (list.ideas) list.ideas = false;
            ImGui::BeginDisabled();
            ImGui::Checkbox("2构想奖励（仅Sarkaz可用）##id_dis", &list.ideas);
            ImGui::EndDisabled();
        }
    }

    // 完整的肉鸽配置界面实现（包含所有配置项）
    void draw_roguelike_config() {
        if (!roguelike_settings_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(800, FLT_MAX));
        if (ImGui::Begin("肉鸽模式配置", &roguelike_settings_open)) {
            // 启用滚动条以适应多内容
            ImGui::BeginChild("ScrollArea", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), false, ImGuiWindowFlags_HorizontalScrollbar);

            // 基础配置区域
            ImGui::Text("基础配置");
            ImGui::Separator();

            // 主题选择
            std::string old_theme = roguelike_config.theme;
            ImGui::SetNextItemWidth(300);
            if (ImGui::BeginCombo("主题##theme",
                [this]() {
                    for (const auto& [id, name] : theme_options) {
                        if (strcmp(roguelike_config.theme, id.c_str()) == 0)
                            return name.c_str();
                    }
                    return "未知主题";
                }())) {
                for (const auto& [id, name] : theme_options) {
                    bool selected = (strcmp(roguelike_config.theme, id.c_str()) == 0);
                    if (ImGui::Selectable(name.c_str(), selected)) {
                        strncpy(roguelike_config.theme, id.c_str(), sizeof(roguelike_config.theme) - 1);
                        roguelike_config.theme[sizeof(roguelike_config.theme) - 1] = '\0';
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (strcmp(old_theme.c_str(), roguelike_config.theme) != 0) {
                on_theme_changed(old_theme, roguelike_config.theme);
            }
            ImGui::Spacing();

            // 模式选择
            ImGui::SetNextItemWidth(300);
            if (ImGui::BeginCombo("模式##mode",
                [this]() {
                    for (const auto& [val, desc] : mode_options) {
                        if (val == roguelike_config.mode) return desc.c_str();
                    }
                    return "未知模式";
                }())) {
                for (const auto& [val, desc] : mode_options) {
                    if (ImGui::Selectable(desc.c_str(), roguelike_config.mode == val)) {
                        roguelike_config.mode = val;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            // 开局分队选择
            ImGui::SetNextItemWidth(300);
            auto squads = get_squad_options(roguelike_config.theme);
            if (ImGui::BeginCombo("开局分队##squad", roguelike_config.squad)) {
                for (const auto& squad : squads) {
                    bool selected = (strcmp(roguelike_config.squad, squad.c_str()) == 0);
                    if (ImGui::Selectable(squad.c_str(), selected)) {
                        strncpy(roguelike_config.squad, squad.c_str(), sizeof(roguelike_config.squad) - 1);
                        roguelike_config.squad[sizeof(roguelike_config.squad) - 1] = '\0';
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            // 开局职业组选择
            ImGui::SetNextItemWidth(300);
            auto roles = get_roles_options(roguelike_config.theme);
            if (ImGui::BeginCombo("开局职业组##roles", roguelike_config.roles)) {
                for (const auto& role : roles) {
                    bool selected = (strcmp(roguelike_config.roles, role.c_str()) == 0);
                    if (ImGui::Selectable(role.c_str(), selected)) {
                        strncpy(roguelike_config.roles, role.c_str(), sizeof(roguelike_config.roles) - 1);
                        roguelike_config.roles[sizeof(roguelike_config.roles) - 1] = '\0';
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();

            // 开局干员名
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("开局干员名##core", roguelike_config.core_char, IM_ARRAYSIZE(roguelike_config.core_char));
            ImGui::TextDisabled("仅支持单个干员中文名，留空则自动选择");
            ImGui::Spacing();

            // 助战设置
            ImGui::Checkbox("使用助战干员##support", &roguelike_config.use_support);

            if (!roguelike_config.use_support) ImGui::BeginDisabled();
            ImGui::SameLine(200);
            ImGui::Checkbox("允许非好友助战##nonfriend", &roguelike_config.use_nonfriend_support);
            if (!roguelike_config.use_support) ImGui::EndDisabled();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // 运行控制区域
            ImGui::Text("运行控制");
            ImGui::Separator();

            // 开始探索次数
            ImGui::SetNextItemWidth(300);
            ImGui::InputInt("开始探索次数##starts_count", &roguelike_config.starts_count);
            ImGui::TextDisabled("达到次数后自动停止任务，默认999");
            ImGui::Spacing();

            // 难度设置（除Phantom外可编辑）
            if (strcmp(roguelike_config.theme, "Phantom") == 0) ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(300);
            ImGui::InputInt("指定难度等级##diff", &roguelike_config.difficulty);
            if (strcmp(roguelike_config.theme, "Phantom") == 0) {
                ImGui::EndDisabled();
                ImGui::TextDisabled("Phantom主题不支持难度设置");
            }
            else {
                ImGui::TextDisabled("若未解锁难度，则使用已解锁的最高难度");
            }
            ImGui::Spacing();

            // 第五层停止（除Phantom外可编辑）
            if (strcmp(roguelike_config.theme, "Phantom") == 0) ImGui::BeginDisabled();
            ImGui::Checkbox("第五层险路恶敌前停止##stop_boss", &roguelike_config.stop_at_final_boss);
            if (strcmp(roguelike_config.theme, "Phantom") == 0) ImGui::EndDisabled();
            ImGui::Spacing();

            // 等级满后停止
            ImGui::Checkbox("肉鸽等级刷满后停止##stop_max_level", &roguelike_config.stop_at_max_level);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // 投资相关设置
            ImGui::Text("投资设置");
            ImGui::Separator();

            ImGui::Checkbox("启用源石锭投资##investment_enabled", &roguelike_config.investment_enabled);

            if (!roguelike_config.investment_enabled) ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(300);
            ImGui::InputInt("最大投资次数##investments_count", &roguelike_config.investments_count);
            ImGui::TextDisabled("达到次数后自动停止任务，默认无上限");
            ImGui::Spacing();

            ImGui::Checkbox("投资到达上限后停止##stop_when_full", &roguelike_config.stop_when_investment_full);
            ImGui::Spacing();

            // 投资后购物（仅模式1）
            if (roguelike_config.mode != 1) ImGui::BeginDisabled();
            ImGui::Checkbox("投资后尝试购物##investment_shopping", &roguelike_config.investment_with_more_score);
            if (roguelike_config.mode != 1) ImGui::EndDisabled();
            if (roguelike_config.mode != 1) {
                ImGui::TextDisabled("仅模式1（刷源石锭）有效");
            }
            if (!roguelike_config.investment_enabled) ImGui::EndDisabled();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // 凹开局设置（仅模式4显示）
            if (roguelike_config.mode == 4) {
                ImGui::Text("凹开局设置");
                ImGui::Separator();

                ImGui::Checkbox("同时凹干员精二直升##elite2", &roguelike_config.start_with_elite_two);

                if (!roguelike_config.start_with_elite_two) ImGui::BeginDisabled();
                ImGui::SameLine(200);
                ImGui::Checkbox("只凹精二直升##only_elite2", &roguelike_config.only_start_with_elite_two);
                if (!roguelike_config.start_with_elite_two) ImGui::EndDisabled();

                // 凹开局奖励列表
                ImGui::Spacing();
                draw_collectible_editor(roguelike_config.collectible_mode_start_list, roguelike_config.theme);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }

            // 主题专属设置
            ImGui::Text("主题专属设置");
            ImGui::Separator();

            // Mizuki专属设置
            if (strcmp(roguelike_config.theme, "Mizuki") == 0) {
                ImGui::Checkbox("用骰子刷新商店（刷指路鳞）##refresh", &roguelike_config.refresh_trader_with_dice);
                ImGui::Spacing();
            }

            // Sami专属设置
            if (strcmp(roguelike_config.theme, "Sami") == 0) {
                ImGui::Checkbox("使用密文板##foldartal", &roguelike_config.use_foldartal);
                ImGui::Checkbox("检测坍缩范式##paradigms", &roguelike_config.check_collapsal_paradigms);

                if (!roguelike_config.check_collapsal_paradigms) ImGui::BeginDisabled();
                ImGui::SameLine(200);
                ImGui::Checkbox("双重检测防漏##double_check", &roguelike_config.double_check_collapsal_paradigms);
                if (!roguelike_config.check_collapsal_paradigms) ImGui::EndDisabled();
                ImGui::Spacing();
            }

            // 其他主题提示
            if (strcmp(roguelike_config.theme, "Mizuki") != 0 &&
                strcmp(roguelike_config.theme, "Sami") != 0) {
                ImGui::TextDisabled("当前主题无专属设置");
                ImGui::Spacing();
            }
            ImGui::Separator();
            ImGui::Spacing();

            // 自动切换设置
            ImGui::Text("自动切换设置");
            ImGui::Separator();

            ImGui::Checkbox("月度小队自动切换##monthly_auto", &roguelike_config.monthly_squad_auto_iterate);
            if (roguelike_config.monthly_squad_auto_iterate) {
                ImGui::SameLine(200);
                ImGui::Checkbox("将通信作为切换依据##monthly_comms", &roguelike_config.monthly_squad_check_comms);
            }
            ImGui::Spacing();

            ImGui::Checkbox("深入调查自动切换##deep_auto", &roguelike_config.deep_exploration_auto_iterate);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // 烧水相关配置
            ImGui::Text("烧水相关配置");
            ImGui::Separator();

            ImGui::Checkbox("烧水时启用购物##shopping", &roguelike_config.collectible_mode_shopping);
            ImGui::Spacing();

            // 烧水使用的分队
            ImGui::SetNextItemWidth(300);
            if (ImGui::BeginCombo("烧水时使用的分队##collect_squad", roguelike_config.collectible_mode_squad)) {
                for (const auto& squad : squads) {
                    bool selected = (strcmp(roguelike_config.collectible_mode_squad, squad.c_str()) == 0);
                    if (ImGui::Selectable(squad.c_str(), selected)) {
                        strncpy(roguelike_config.collectible_mode_squad, squad.c_str(),
                            sizeof(roguelike_config.collectible_mode_squad) - 1);
                        roguelike_config.collectible_mode_squad[sizeof(roguelike_config.collectible_mode_squad) - 1] = '\0';
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("默认与开局分队同步");
            ImGui::Spacing();

            // 烧水期望奖励
            draw_collectible_editor(roguelike_config.collectible_mode_collectibles, roguelike_config.theme);
            ImGui::Spacing();

            ImGui::EndChild();

            // 保存/取消按钮
            if (ImGui::Button("保存配置##save")) {
                save_config();
                ImGui::OpenPopup("提示##saved");
            }
            ImGui::SameLine();
            if (ImGui::Button("取消##cancel")) {
                roguelike_settings_open = false;
            }

            // 保存提示弹窗
            if (ImGui::BeginPopup("提示##saved")) {
                ImGui::Text("配置已保存");
                if (ImGui::Button("确定##ok")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }


    void draw_settings_window() {
        if (!settings_window_open) return;

        ImGui::SetNextWindowSizeConstraints(ImVec2(window_min_width, 300), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(window_min_width, 350), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("设置", &settings_window_open)) {
            ImGui::Text("配置项列表");
            ImGui::Separator();

            // 启动参数设置入口
            ImGui::BeginGroup();
            ImGui::Checkbox("##main_startup_enable", &startup_config.enable);
            ImGui::SameLine();
            draw_label("启动参数配置");
            if (ImGui::Button("配置##main_startup_btn")) {
                startup_settings_open = true;
            }
            ImGui::EndGroup();
            ImGui::Spacing();

            // 关卡任务设置入口
            ImGui::BeginGroup();
            ImGui::Checkbox("##main_stage_enable", &stage_config.enable);
            ImGui::SameLine();
            draw_label("关卡任务配置");
            if (ImGui::Button("配置##main_stage_btn")) {
                stage_settings_open = true;
            }
            ImGui::EndGroup();
            ImGui::Spacing();

            // 公招配置入口
            ImGui::BeginGroup();
            ImGui::Checkbox("##main_recruitment_enable", &recruitment_config.enable);
            ImGui::SameLine();
            draw_label("公招配置");
            if (ImGui::Button("配置##main_recruitment_btn")) {
                recruitment_settings_open = true;
            }
            ImGui::EndGroup();
            ImGui::Spacing();

            // 新增：设施配置入口
            ImGui::BeginGroup();
            ImGui::Checkbox("##main_facility_enable", &facility_config.enable);
            ImGui::SameLine();
            draw_label("设施换班配置");
            if (ImGui::Button("配置##main_facility_btn")) {
                facility_settings_open = true;
            }
            ImGui::EndGroup();

            // 新增：购物配置入口
            ImGui::BeginGroup();
            ImGui::Checkbox("##main_shopping_enable", &shopping_config.enable);
            ImGui::SameLine();
            draw_label("商店购物配置");
            if (ImGui::Button("配置##main_shopping_btn")) {
                shopping_settings_open = true;
            }
            ImGui::EndGroup();
            ImGui::Spacing();

            // 新增：任务配置入口
            ImGui::BeginGroup();
            ImGui::Checkbox("##main_mission_enable", &mission_config.enable);
            ImGui::SameLine();
            draw_label("任务奖励配置");
            if (ImGui::Button("配置##main_mission_btn")) {
                mission_settings_open = true;
            }
            ImGui::EndGroup();
            ImGui::Spacing();

            ImGui::BeginGroup();
            ImGui::Checkbox("##roguelike_toggle", &roguelike_config.enable);
            ImGui::SameLine();
            draw_label("肉鸽模式配置");
            if (ImGui::Button("配置##roguelike_btn")) {
                roguelike_settings_open = true;
            }
            ImGui::EndGroup();
            ImGui::Spacing();

            ImGui::Separator();
            ImGui::TextDisabled("勾选框控制功能启用状态");

            if (ImGui::Button("关闭##main_window")) {
                settings_window_open = false;
            }
        }
        ImGui::End();

        draw_startup_subpage();
        draw_stage_subpage();
        draw_recruitment_subpage();
        draw_facility_subpage();  // 绘制设施配置页面
        draw_shopping_subpage();  // 绘制购物配置页面
        draw_mission_subpage();  // 绘制任务配置页面
        draw_roguelike_config();
    }
};
SettingsManager settings("config.json");

namespace fs = std::filesystem;
fs::path exec_dir;
fs::path dbg_dir;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void SetupImGuiFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    // 获取Windows字体目录
    char winDir[MAX_PATH];
    GetWindowsDirectoryA(winDir, MAX_PATH);
    std::string fontsDir = std::string(winDir) + "\\Fonts\\";

    // 主字体：英文+中文+常用符号
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false; // 防止重复释放

    // 尝试加载微软雅黑（中文）
    std::string yaheiPath = fontsDir + "msyh.ttc";
    ImFont* mainFont = io.Fonts->AddFontFromFileTTF(
        yaheiPath.c_str(),
        18.0f,
        &config,
        io.Fonts->GetGlyphRangesChineseFull()
    );

    // 备用方案：如果雅黑不存在，使用系统默认
    if (!mainFont) {
        mainFont = io.Fonts->AddFontDefault();
    }

    // 合并日文字体
    config.MergeMode = true; // 启用合并模式

    // 优先尝试Yu Gothic UI (Win8.1+)
    std::string yuGothicPath = fontsDir + "YuGothm.ttc";
    if (GetFileAttributesA(yuGothicPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        io.Fonts->AddFontFromFileTTF(
            yuGothicPath.c_str(),
            18.0f,
            &config,
            io.Fonts->GetGlyphRangesJapanese()
        );
    }
    // 回退到MS UI Gothic (WinXP+)
    else {
        std::string uiGothicPath = fontsDir + "msgothic.ttc";
        io.Fonts->AddFontFromFileTTF(
            uiGothicPath.c_str(),
            18.0f,
            &config,
            io.Fonts->GetGlyphRangesJapanese()
        );
    }

    io.FontDefault = mainFont;
}

// 获取当前EXE所在的目录
fs::path get_exe_directory() {
    // 初始缓冲区大小，可根据需要调整
    DWORD buffer_size = MAX_PATH;
    std::wstring path_buffer(buffer_size, L'\0');

    // 循环获取完整路径（处理长路径情况）
    while (true) {
        DWORD copied = GetModuleFileNameW(nullptr, &path_buffer[0], buffer_size);

        if (copied == 0) {
            // 获取失败，抛出异常
            throw std::runtime_error("无法获取EXE路径，错误码: " + std::to_string(GetLastError()));
        }

        if (copied < buffer_size) {
            // 成功获取，截断多余字符
            path_buffer.resize(copied);
            break;
        }

        // 缓冲区不足，扩大缓冲区重试
        buffer_size *= 2;
        if (buffer_size > 32768) {  // 防止无限扩大
            throw std::runtime_error("EXE路径过长，超出处理范围");
        }
        path_buffer.resize(buffer_size, L'\0');
    }

    // 转换为filesystem路径并返回父目录
    return fs::path(path_buffer).parent_path();
}

// 在全局变量区域添加
enum AppState {
    STATE_INITIALIZING = 0,
    STATE_MAIN_MENU,
    STATE_RUNNING_TASK,
    STATE_TASK_SUCCESS,
    STATE_STOPPING
};

AppState g_currentState = STATE_INITIALIZING;
ID3D11ShaderResourceView* g_backgroundTexture = nullptr;
float g_taskProgress = 0.0f; // 用于状态0的进度
std::string g_taskError = "";
std::string g_taskInfo = "";
std::string g_taskInfoEx = "";

std::atomic<bool> g_taskRunning(false); // 线程运行标志
cv::Mat g_cachedBackground; // OpenCV缓存背景图

// 背景图片加载函数
bool LoadBackgroundTexture(ID3D11Device* device) {
    cv::Mat img;

    // 尝试从磁盘加载
    try {
        img = cv::imread("background.png");
    }
    catch (...) {
        img = cv::Mat();
    }

    /*
    // 如果加载失败，使用外部资源
    if (img.empty()) {
        // 假设有外部定义的png资源
        extern const unsigned char* EMBEDDED_BACKGROUND_DATA;
        extern const size_t EMBEDDED_BACKGROUND_SIZE;

        if (EMBEDDED_BACKGROUND_DATA && EMBEDDED_BACKGROUND_SIZE > 0) {
            cv::_InputArray buf(EMBEDDED_BACKGROUND_DATA, EMBEDDED_BACKGROUND_SIZE);
            img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
        }
    }
    */
    // 如果仍然失败，创建默认背景
    if (img.empty()) {
        img = cv::Mat(450, 450, CV_8UC3, cv::Scalar(30, 30, 30));
    }

    // 调整大小为450x450
    if (img.cols != 450 || img.rows != 450) {
        cv::resize(img, img, cv::Size(450, 450));
    }

    // 暗淡化处理 (0.3倍亮度)
    img.convertTo(img, -1, 0.3);

    // 缓存OpenCV副本
    g_cachedBackground = img.clone();

    // 创建DX11纹理
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = img.cols;
    desc.Height = img.rows;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = img.data;
    subResource.SysMemPitch = img.step;
    subResource.SysMemSlicePitch = 0;

    ID3D11Texture2D* pTexture = nullptr;
    if (device->CreateTexture2D(&desc, &subResource, &pTexture) != S_OK) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (device->CreateShaderResourceView(pTexture, &srvDesc, &g_backgroundTexture) != S_OK) {
        pTexture->Release();
        return false;
    }

    pTexture->Release();
    return true;
}

// 弹窗提示用户
void show_message(const std::string& title, const std::string& message) {
    MessageBoxA(
        nullptr,               // 父窗口句柄，nullptr表示无父窗口
        message.c_str(),       // 消息内容
        title.c_str(),         // 窗口标题
        MB_OK | MB_ICONINFORMATION  // 按钮样式和图标
    );
}

// 状态处理函数
void RenderAppState() {
    ImGuiIO& io = ImGui::GetIO();

    // 1. 首先绘制背景（最低层级）
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Background", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground
    ); // 确保背景在最底层

    if (g_backgroundTexture) {
        // 保持宽高比拉伸填充
        float aspect = 450.0f / 450.0f; // 原始宽高比
        ImVec2 imageSize = io.DisplaySize;

        // 计算保持宽高比的尺寸
        float windowAspect = imageSize.x / imageSize.y;
        if (windowAspect > aspect) {
            // 窗口更宽，高度填充
            imageSize.x = imageSize.y * aspect;
        }
        else {
            // 窗口更高，宽度填充
            imageSize.y = imageSize.x / aspect;
        }

        // 居中显示
        ImVec2 pos = ImVec2(
            (io.DisplaySize.x - imageSize.x) * 0.5f,
            (io.DisplaySize.y - imageSize.y) * 0.5f
        );

        ImGui::SetCursorPos(pos);
        ImGui::Image(g_backgroundTexture, imageSize);
    }
    ImGui::End();

    // 状态特定UI
    switch (g_currentState) {
    case STATE_INITIALIZING:
        RenderInitializingState();
        break;
    case STATE_MAIN_MENU:
        RenderMainMenuState();
        break;
    case STATE_RUNNING_TASK:
        RenderRunningTaskState();
        break;
    case STATE_TASK_SUCCESS:
        RenderTaskSuccessState();
        break;
    case STATE_STOPPING:
        RenderStoppingState();
        break;
    }
}

// 在所有状态函数中，将:
// ImVec2 center = io.DisplaySize * 0.5f;
// 替换为手动计算每个分量:

void RenderInitializingState() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Initializing", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove);

    // 修正：手动计算中心位置
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    ImGui::SetCursorPos(ImVec2(centerX - 150, centerY - 20));
    ImGui::Text(g_taskInfo.c_str());
    //g_taskError
    if (g_taskError != "") {
        ImGui::SetCursorPos(ImVec2(centerX - 150, centerY));
        ImGui::TextColored(ImVec4(1, 1, 0, 1), g_taskError.c_str());
    }
    else {
        ImGui::SetCursorPos(ImVec2(centerX - 150, centerY));
        ImGui::ProgressBar(g_taskProgress, ImVec2(300, 20));
    }

    ImGui::SetCursorPos(ImVec2(centerX - 150, centerY + 20));
    ImGui::Text(g_taskInfoEx.c_str());

    ImGui::SetCursorPos(ImVec2(centerX - 180, centerY - 4));
    ImGui::Spinner("##spinner", 10, 2, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

    ImGui::End();
}

void RenderMainMenuState() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Main Menu", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus);


    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("开始！")) {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("设置")) {
            settings.draw_settings_button();
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
    // 在ImGui主循环中调用
    // settings_window_open = true;
    //settings.draw_settings_button();  // 绘制打开设置的按钮
    settings.draw_settings_window();  // 绘制设置窗口（需在ImGui::BeginFrame之后）

    ImGui::End();
}

void RenderRunningTaskState() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Processing", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove);

    // 修正：手动计算中心位置
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    ImGui::SetCursorPos(ImVec2(centerX - 100, centerY - 30));
    ImGui::Text("Task in progress...");

    ImGui::SetCursorPos(ImVec2(centerX - 15, centerY + 10));
    ImGui::Spinner("##spinner", 30, 6, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

    ImGui::End();
}

void RenderTaskSuccessState() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Success", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove);

    // 修正：手动计算中心位置
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    ImGui::SetCursorPos(ImVec2(centerX - 90, centerY - 20));
    if (ImGui::Button("Stop", ImVec2(180, 40))) {
        // ... 按钮逻辑
    }

    ImGui::End();
}

void RenderStoppingState() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Stopping", nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove);

    // 修正：手动计算中心位置
    float centerX = io.DisplaySize.x * 0.5f;
    float centerY = io.DisplaySize.y * 0.5f;

    ImGui::SetCursorPos(ImVec2(centerX - 150, centerY - 30));
    ImGui::Text("Stopping task, please wait...");

    ImGui::SetCursorPos(ImVec2(centerX - 15, centerY + 10));
    ImGui::Spinner("##spinner", 30, 6, ImGui::GetColorU32(ImGuiCol_ButtonHovered));

    ImGui::End();
}

// 初始化函数（带进度回调）
void maa_initiating(std::function<void(float)> progressCallback) {
    g_taskError = "";
    g_taskInfo = "正在验证MAA基本资源";
    g_taskInfoEx = "";
    try {
        fs::path exe_dir = get_exe_directory();
        exec_dir = exe_dir;
        g_taskInfoEx = exec_dir.string();
    }
    catch (const std::exception& e) {
        g_taskError = "请通过常规方式打开RSS,无工作目录的情况下RSS不能运行。";
        Sleep(5000);
        exit(-1);
    }
    dbg_dir = exec_dir / "debug";
    AsstSetUserDir(dbg_dir.string().c_str());

    if (!AsstLoadResource(exec_dir.string().c_str())) {
        g_taskError = "基本 resource 丢失或不完整。";
        Sleep(5000);
        exit(-1);
    }

    g_taskInfo = "正在加载配置文件";
    g_taskInfoEx = "";
    g_taskError = "";

    progressCallback(1.00f); // 更新进度
}

// 第二段任务（返回是否成功）
bool maa_loop() {
    // 你的代码...
    return true; // 或 false
}

// 第三段任务
void maa_return() {
    // 你的停止代码...
}

// 在全局区域添加
void UpdateInitializationProgress(float progress) {
    g_taskProgress = progress;
    // 当进度完成时切换到主菜单
    if (g_taskProgress >= 1.0f) {
        g_currentState = STATE_MAIN_MENU;
    }
}

int main(int, char**)
{
    system("chcp 65001");
    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"RSS", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"RSF", WS_OVERLAPPEDWINDOW, 100, 100, (int)(950), (int)(540), nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    LoadBackgroundTexture(g_pd3dDevice);

    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    SetupImGuiFonts();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // 在WinMain的初始化部分添加（在加载背景后）
// 启动初始化线程
    std::thread([]() {
        // 这里执行你的第一段初始化代码
        maa_initiating([](float progress) {
            UpdateInitializationProgress(progress);
            });
        }).detach();

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            RenderAppState();
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            // 释放旧背景纹理
            if (g_backgroundTexture) {
                g_backgroundTexture->Release();
                g_backgroundTexture = nullptr;
            }

            // 重建渲染目标
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();

            // 重新加载背景纹理（适应新窗口大小）
            LoadBackgroundTexture(g_pd3dDevice);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
