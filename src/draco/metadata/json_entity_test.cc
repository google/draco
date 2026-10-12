// Copyright 2024 The Draco Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "draco/metadata/json_entity.h"

#include <string>
#include <vector>

#include "draco/core/draco_test_utils.h"

namespace {

#ifdef DRACO_TRANSCODER_SUPPORTED

TEST(JsonEntityTest, JsonEntityDefaultConstructor) {
  // Test construction of an empty entity.
  draco::JsonEntity entity;
  ASSERT_TRUE(entity.GetName().empty());
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::OBJECT);
  ASSERT_TRUE(entity.GetObject().empty());
  ASSERT_TRUE(entity.GetArray().empty());
  ASSERT_TRUE(entity.GetString().empty());
  ASSERT_EQ(entity.GetInteger(), 0);
  ASSERT_EQ(entity.GetDouble(), 0.0);
  ASSERT_FALSE(entity.GetBoolean());
}

TEST(JsonEntityTest, JsonEntityNamedConstructor) {
  // Test construction of a named entity.
  draco::JsonEntity entity("Flexible Demeanour");
  ASSERT_EQ(entity.GetName(), "Flexible Demeanour");
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::OBJECT);
  ASSERT_TRUE(entity.GetObject().empty());
}

TEST(JsonEntityTest, JsonEntityStringConstructor) {
  // Test construction of entity storing a string.
  draco::JsonEntity entity("Flexible Demeanour", "GCU");
  ASSERT_EQ(entity.GetName(), "Flexible Demeanour");
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::STRING);
  ASSERT_EQ(entity.GetString(), "GCU");
}

TEST(JsonEntityTest, JsonEntityIntegerConstructor) {
  // Test construction of entity storing an integer.
  draco::JsonEntity entity("Flexible Demeanour", 12);
  ASSERT_EQ(entity.GetName(), "Flexible Demeanour");
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::INTEGER);
  ASSERT_EQ(entity.GetInteger(), 12);
}

TEST(JsonEntityTest, JsonEntityDoubleConstructor) {
  // Test construction of entity storing a double.
  draco::JsonEntity entity("Flexible Demeanour", 1.2);
  ASSERT_EQ(entity.GetName(), "Flexible Demeanour");
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::DOUBLE);
  ASSERT_EQ(entity.GetDouble(), 1.2);
}

TEST(JsonEntityTest, JsonEntityBooleanConstructor) {
  // Test construction of entity storing a boolean.
  draco::JsonEntity entity("Flexible Demeanour", true);
  ASSERT_EQ(entity.GetName(), "Flexible Demeanour");
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::BOOLEAN);
  ASSERT_TRUE(entity.GetBoolean());
}

TEST(JsonEntityTest, JsonEntitySettersAndGetters) {
  // Test value setters and getters of entity.
  draco::JsonEntity entity;
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::OBJECT);

  entity.SetArray().push_back(draco::JsonEntity("entry", 12));
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::ARRAY);
  ASSERT_EQ(entity.GetArray().size(), 1);
  ASSERT_EQ(entity.GetArray()[0].GetName(), "entry");
  ASSERT_EQ(entity.GetArray()[0].GetInteger(), 12);

  entity.SetObject().push_back(draco::JsonEntity("entity", 9));
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::OBJECT);
  ASSERT_EQ(entity.GetObject().size(), 1);
  ASSERT_EQ(entity.GetObject()[0].GetName(), "entity");
  ASSERT_EQ(entity.GetObject()[0].GetInteger(), 9);

  entity.SetString("matter");
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::STRING);
  ASSERT_EQ(entity.GetString(), "matter");

  entity.SetInteger(5);
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::INTEGER);
  ASSERT_EQ(entity.GetInteger(), 5);

  entity.SetDouble(1.5);
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::DOUBLE);
  ASSERT_EQ(entity.GetDouble(), 1.5);

  entity.SetBoolean(true);
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::BOOLEAN);
  ASSERT_EQ(entity.GetBoolean(), true);
}

TEST(JsonEntityTest, JsonEntityLookupByName) {
  // Test the GetObjectByName() getter.
  draco::JsonEntity entity;
  ASSERT_EQ(entity.GetType(), draco::JsonEntity::OBJECT);

  auto& entities = entity.SetObject();
  entities.push_back(draco::JsonEntity("entity1", 1));
  entities.push_back(draco::JsonEntity("entity2", "two"));

  draco::JsonEntity entity3("entity3");
  entity3.SetObject().push_back(draco::JsonEntity("child_entity", "child"));
  entities.push_back(entity3);

  ASSERT_EQ(entity.GetObjectEntityByName("child_entity"), nullptr);

  ASSERT_NE(entity.GetObjectEntityByName("entity1"), nullptr);
  ASSERT_EQ(entity.GetObjectEntityByName("entity1")->GetInteger(), 1);

  ASSERT_NE(entity.GetObjectEntityByName("entity2"), nullptr);
  ASSERT_EQ(entity.GetObjectEntityByName("entity2")->GetString(), "two");

  ASSERT_NE(entity.GetObjectEntityByName("entity3"), nullptr);
  ASSERT_NE(entity.GetObjectEntityByName("entity3")->GetObjectEntityByName(
                "child_entity"),
            nullptr);
  ASSERT_EQ(entity.GetObjectEntityByName("entity3")
                ->GetObjectEntityByName("child_entity")
                ->GetString(),
            "child");
}

TEST(JsonEntityTest, JsonEntityCompare) {
  // Test comparison of two JSON entities.
  {
    // Compare the same entity.
    draco::JsonEntity a;
    ASSERT_TRUE(a == a);
    ASSERT_FALSE(a != a);
  }
  {
    // Compare two default entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
  }
  {
    // Compare two entities with different names.
    draco::JsonEntity a("one");
    draco::JsonEntity b("two");
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two entities with different types.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetInteger(1);
    b.SetString("one");
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two identical string-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetString("one");
    b.SetString("one");
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
  }
  {
    // Compare two different string-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetString("one");
    b.SetString("two");
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two identical integer-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetInteger(1);
    b.SetInteger(1);
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
  }
  {
    // Compare two different integer-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetInteger(1);
    b.SetInteger(2);
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two identical double-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetDouble(1.1);
    b.SetDouble(1.1);
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
  }
  {
    // Compare two different double-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetDouble(1.1);
    b.SetDouble(1.2);
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two identical boolean-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetBoolean(true);
    b.SetBoolean(true);
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
  }
  {
    // Compare two different boolean-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetBoolean(true);
    b.SetBoolean(false);
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two identical entity-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetObject().emplace_back("one");
    b.SetObject().emplace_back("one");
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
  }
  {
    // Compare two different entity-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetObject().emplace_back("one");
    b.SetObject().emplace_back("two");
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two entity-type entities with different counts.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetObject().emplace_back("one");
    b.SetObject().emplace_back("one");
    b.SetObject().emplace_back("two");
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two identical array-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetArray().emplace_back("", 1);
    b.SetArray().emplace_back("", 1);
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
  }
  {
    // Compare two different array-type entities.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetArray().emplace_back("", 1);
    b.SetArray().emplace_back("", 2);
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
  {
    // Compare two array-type entities with different counts.
    draco::JsonEntity a;
    draco::JsonEntity b;
    a.SetArray().emplace_back("", 1);
    b.SetArray().emplace_back("", 1);
    b.SetArray().emplace_back("", 2);
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
  }
}

#endif  // DRACO_TRANSCODER_SUPPORTED

}  // namespace
