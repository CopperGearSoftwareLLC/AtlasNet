#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/global_fun.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/describe.hpp>
#include <boost/describe/enum.hpp>
#include <boost/describe/enum_to_string.hpp>
#include <boost/describe/enum_from_string.hpp>
#include <boost/stacktrace.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/static_string.hpp>

template<std::size_t N>
using static_string = boost::static_string<N>;


#include "nlohmann/json.hpp"
using Json = nlohmann::json;
using Ordered_Json = nlohmann::ordered_json;

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/string_cast.hpp"
using mat2 = glm::mat2;
using mat3 = glm::mat3;
using mat4 = glm::mat4;
using uvec2 = glm::uvec2;
using uvec3 = glm::uvec3;
using uvec4 = glm::uvec4;
using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;
using ivec4 = glm::ivec4;
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using quat = glm::quat;
template <glm::length_t L, typename T>
using vec = glm::vec<L, T>;



#define ASSERT(check,message) assert(check && message)
