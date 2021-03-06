﻿/*******************************************************************************
 * 文件： random_generator.cpp
 * 时间： 2014/12/31 14:27
 * 作者： 冯兵
 * 邮件： fengbing123@gmail.com
 *
 * 说明： 随机数生成
 *
********************************************************************************/
#include "base_precomp.h"
#include <mvg/random/random_generator.h>



//定义一个全局的随机数生成器
mvg::random::RandomGenerator mvg::random::randomGenerator;

void mvg::random::RandomGenerator::seed()
{
	engine.seed(random_device());
}

int mvg::random::RandomGenerator::randomIntWithinRange(int minValue, int maxValue)
{
	std::uniform_int_distribution< int > dist(minValue, maxValue);
	return dist(engine); 
}

float mvg::random::RandomGenerator::randomFloatWithinRange(float minValue, float maxValue)
{
	std::uniform_real_distribution< float > dist(minValue, maxValue);
	return dist(engine);
}