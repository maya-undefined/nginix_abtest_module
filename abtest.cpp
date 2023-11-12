extern "C" {
    #include <ngx_config.h>
    #include <ngx_core.h>
    #include <ngx_http.h>
}
#include <yaml-cpp/yaml.h>
#include <random>
#include <mutex>

// pretend this comes from a redis cluster
const char* embeddedYAML = R"(
buckets:
  -optionA: /option_A
  -optionB: /option_B
  -optionC: /option_C
experiment_chance: 5
default: /
)";

class ABTester {
private:
    static std::unique_ptr<ABTester> instance;
    ABTester() {}

public:
    ABTester(const ABTester&) = delete;
    ABTester(ABTester&&) = delete;
    ABTester& operator=(const ABTester&) = delete;
    ABTester& operator=(ABTester&&) = delete;

    static ABTester& getInstance() {
        static ABTester instance;
        return instance;
    }

    static std::string determineBucket(const YAML::Node& yaml_config, std::string x_test_group) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.0, 1.0);
            
            double randomValue = dis(gen);
            double experiment_chance = yaml_config["experiment_chance"].as<float>() / 100;

            if (experiment_chance < randomValue) {
                if (yaml_config["buckets"] && yaml_config["buckets"][x_test_group]) {
                    return yaml_config["buckets"][x_test_group].as<std::string>();
                }
            }
            return yaml_config["default"].as<std::string>();
    }
};


class ABTestHandler {
public:
    static ngx_int_t handle_request(ngx_http_request_t* r) {
        std::istringstream yamlStream(embeddedYAML);
        YAML::Node yaml_config = YAML::Load(yamlStream);

        std::string x_test_group = "X-Test-Group";
        // need to get the header value outj of request

        ABTester& tester = ABTester::getInstance();

        std::string a = tester.determineBucket(yaml_config, x_test_group);
        ngx_str_t url_target;
        url_target.data = reinterpret_cast<u_char*>(a.data());
        url_target.len = a.length();

        r->headers_out.status = NGX_HTTP_MOVED_TEMPORARILY; // 302 redirect
        r->headers_out.location = (ngx_table_elt_t*)ngx_list_push(&r->headers_out.headers);
        // ngx_str_set(&r->headers_out.location->key, "Location");

        r->headers_out.location->value = url_target;

        return ngx_http_send_header(r);
    }
};

extern "C" {
    static char* ngx_http_set_cpp_ab(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
    static ngx_int_t ngx_http_cpp_ab_handler(ngx_http_request_t* r);

    static ngx_command_t ngx_http_cpp_ab_commands[] = {
        {
            ngx_string("ab_module"),
            NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
            ngx_http_set_cpp_ab,
            0,
            0,
            NULL
        },

        ngx_null_command
    };

    static ngx_http_module_t ngx_http_cpp_ab_module_ctx = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    };

    ngx_module_t ab_module = {
        NGX_MODULE_V1,
        &ngx_http_cpp_ab_module_ctx,
        ngx_http_cpp_ab_commands,
        NGX_HTTP_MODULE,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NGX_MODULE_V1_PADDING
    };

    static char* ngx_http_set_cpp_ab(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
        ngx_http_core_loc_conf_t* clcf;
        clcf = (ngx_http_core_loc_conf_t*)ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
        clcf->handler = ngx_http_cpp_ab_handler;
        return NGX_CONF_OK;
    }

    static ngx_int_t ngx_http_cpp_ab_handler(ngx_http_request_t* r) {
        return ABTestHandler::handle_request(r);
    }
}
