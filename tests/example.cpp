// Copyright 2022 VladislavRz <rzhevskii_vladislav@mail.ru>

#include <stdexcept>
#include <gtest/gtest.h>
#include <example.hpp>

TEST(Example, EmptyTest) {
    EXPECT_THROW(example(), std::runtime_error);
}
