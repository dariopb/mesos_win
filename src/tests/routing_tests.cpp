/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <signal.h>
#include <unistd.h>

#include <linux/version.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <gtest/gtest.h>

#include <stout/foreach.hpp>
#include <stout/gtest.hpp>
#include <stout/hashmap.hpp>
#include <stout/net.hpp>

#include "linux/routing/link/link.hpp"

using namespace routing;

using std::set;
using std::string;


static const string TEST_VETH_LINK = "veth-test";
static const string TEST_PEER_LINK = "veth-peer";


class RoutingTest : public ::testing::Test {};


// Tests that require setting up virtual ethernet on host.
class RoutingVethTest : public RoutingTest
{
protected:
  virtual void SetUp()
  {
    // Clean up the test links, in case it wasn't cleaned up properly
    // from previous tests.
    link::remove(TEST_VETH_LINK);

    ASSERT_SOME_FALSE(link::exists(TEST_VETH_LINK));
    ASSERT_SOME_FALSE(link::exists(TEST_PEER_LINK));
  }

  virtual void TearDown()
  {
    link::remove(TEST_VETH_LINK);
  }
};


TEST_F(RoutingTest, LinkIndex)
{
  Try<set<string> > links = net::links();
  ASSERT_SOME(links);

  foreach (const string& link, links.get()) {
    EXPECT_SOME_NE(0, link::index(link));
  }

  EXPECT_NONE(link::index("not-exist"));
}


TEST_F(RoutingTest, LinkName)
{
  Try<set<string> > links = net::links();
  ASSERT_SOME(links);

  foreach (const string& link, links.get()) {
    EXPECT_SOME_NE(0, link::index(link));
    EXPECT_SOME_EQ(link, link::name(link::index(link).get()));
  }
}


TEST_F(RoutingTest, LinkStatistics)
{
  Try<set<string> > links = net::links();
  ASSERT_SOME(links);

  foreach (const string& link, links.get()) {
    Result<hashmap<string, uint64_t> > statistics = link::statistics(link);

    ASSERT_SOME(statistics);
    EXPECT_TRUE(statistics.get().contains("rx_packets"));
    EXPECT_TRUE(statistics.get().contains("rx_bytes"));
    EXPECT_TRUE(statistics.get().contains("tx_packets"));
    EXPECT_TRUE(statistics.get().contains("tx_bytes"));
  }

  EXPECT_NONE(link::statistics("not-exist"));
}


TEST_F(RoutingTest, LinkExists)
{
  Try<set<string> > links = net::links();
  ASSERT_SOME(links);

  foreach (const string& link, links.get()) {
    EXPECT_SOME_TRUE(link::exists(link));
  }

  EXPECT_SOME_FALSE(link::exists("not-exist"));
}


TEST_F(RoutingVethTest, ROOT_LinkCreate)
{
  ASSERT_SOME(link::create(TEST_VETH_LINK, TEST_PEER_LINK, None()));

  EXPECT_SOME_TRUE(link::exists(TEST_VETH_LINK));
  EXPECT_SOME_TRUE(link::exists(TEST_PEER_LINK));

  EXPECT_SOME_NE(0, link::index(TEST_VETH_LINK));
  EXPECT_SOME_NE(0, link::index(TEST_PEER_LINK));

  // Test the case where the veth (with the same name) already exists.
  EXPECT_SOME_FALSE(link::create(TEST_VETH_LINK, TEST_PEER_LINK, None()));
}


TEST_F(RoutingVethTest, ROOT_LinkRemove)
{
  ASSERT_SOME(link::create(TEST_VETH_LINK, TEST_PEER_LINK, None()));

  EXPECT_SOME_TRUE(link::remove(TEST_VETH_LINK));
  EXPECT_SOME_FALSE(link::remove(TEST_VETH_LINK));
  EXPECT_SOME_FALSE(link::remove(TEST_PEER_LINK));
}


// Entry point of the child process (used in clone()).
static int child(void*)
{
  // Wait to be killed.
  while (true) {
    sleep(1);
  }

  // Should not reach here.
  ABORT("Child process should not reach here");

  return -1;
}


// Network namespace is not available until Linux 2.6.24.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
TEST_F(RoutingVethTest, ROOT_LinkCreatePid)
{
  // Stack used in the child process.
  unsigned long long stack[32];

  pid_t pid = ::clone(child, &stack[31], CLONE_NEWNET | SIGCHLD, NULL);
  ASSERT_NE(-1, pid);

  // In parent process.
  ASSERT_SOME(link::create(TEST_VETH_LINK, TEST_PEER_LINK, pid));
  EXPECT_SOME_TRUE(link::exists(TEST_VETH_LINK));

  // The peer should not exist in parent network namespace.
  EXPECT_SOME_FALSE(link::exists(TEST_PEER_LINK));

  // TODO(jieyu): Enter the child network namespace and make sure that
  // the TEST_PEER_LINK is there.

  EXPECT_SOME_NE(0, link::index(TEST_VETH_LINK));

  // Kill the child process.
  ASSERT_NE(-1, kill(pid, SIGKILL));

  // Wait for the child process.
  int status;
  EXPECT_NE(-1, waitpid((pid_t) -1, &status, 0));
  ASSERT_TRUE(WIFSIGNALED(status));
  EXPECT_EQ(SIGKILL, WTERMSIG(status));
}
#endif


TEST_F(RoutingVethTest, ROOT_LinkSetUp)
{
  ASSERT_SOME(link::create(TEST_VETH_LINK, TEST_PEER_LINK, None()));

  EXPECT_SOME_TRUE(link::exists(TEST_VETH_LINK));
  EXPECT_SOME_TRUE(link::exists(TEST_PEER_LINK));

  EXPECT_SOME_FALSE(link::isUp(TEST_VETH_LINK));
  EXPECT_SOME_TRUE(link::setUp(TEST_VETH_LINK));
  EXPECT_SOME_TRUE(link::isUp(TEST_VETH_LINK));

  EXPECT_SOME_FALSE(link::isUp(TEST_PEER_LINK));
  EXPECT_SOME_TRUE(link::setUp(TEST_PEER_LINK));
  EXPECT_SOME_TRUE(link::isUp(TEST_PEER_LINK));

  EXPECT_NONE(link::isUp("non-exist"));
  EXPECT_SOME_FALSE(link::setUp("non-exist"));
}


TEST_F(RoutingVethTest, ROOT_LinkSetMAC)
{
  ASSERT_SOME(link::create(TEST_VETH_LINK, TEST_PEER_LINK, None()));

  EXPECT_SOME_TRUE(link::exists(TEST_VETH_LINK));
  EXPECT_SOME_TRUE(link::exists(TEST_PEER_LINK));

  uint8_t bytes[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};

  EXPECT_SOME_TRUE(link::setMAC(TEST_VETH_LINK, net::MAC(bytes)));
  EXPECT_SOME_TRUE(link::setMAC(TEST_PEER_LINK, net::MAC(bytes)));

  Result<net::MAC> mac = net::mac(TEST_VETH_LINK);

  ASSERT_SOME(mac);
  EXPECT_EQ(mac.get(), net::MAC(bytes));

  mac = net::mac(TEST_PEER_LINK);

  ASSERT_SOME(mac);
  EXPECT_EQ(mac.get(), net::MAC(bytes));

  EXPECT_SOME_FALSE(link::setMAC("non-exist", net::MAC(bytes)));

  // Kernel will reject a multicast MAC address.
  uint8_t multicast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

  EXPECT_ERROR(link::setMAC(TEST_VETH_LINK, net::MAC(multicast)));
}


TEST_F(RoutingVethTest, ROOT_LinkMTU)
{
  ASSERT_SOME(link::create(TEST_VETH_LINK, TEST_PEER_LINK, None()));

  EXPECT_SOME_TRUE(link::exists(TEST_VETH_LINK));
  EXPECT_SOME_TRUE(link::exists(TEST_PEER_LINK));

  EXPECT_SOME_TRUE(link::setMTU(TEST_VETH_LINK, 10000));

  Result<unsigned int> mtu = link::mtu(TEST_VETH_LINK);
  ASSERT_SOME(mtu);
  EXPECT_EQ(10000u, mtu.get());

  EXPECT_NONE(link::mtu("not-exist"));
  EXPECT_SOME_FALSE(link::setMTU("not-exist", 1500));
}