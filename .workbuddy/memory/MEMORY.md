# Venom 项目记忆

## 项目概述
ESP32-S3 (M5Stack StickS3) 上运行的"毒液"生物模拟程序。基于 PlatformIO + Arduino 框架。
屏幕 240×135，Metaball 场分辨率 120×68（FIELD_SCALE=2）。

## 物理系统核心
- 12个节点（MAX_NODES=12），链式弹簧骨骼
- shapeArchetype 形态原型：0=FREE, 1=GROUND, 2=WALL, 3=CEILING, 4=CORNER
- PBD 约束解算器控制节点间距上限
- 切向扩散力（SPREAD_FORCE_MAG）控制节点沿墙铺展

## 关键参数（当前值）
- SPREAD_FORCE_MAG = 5.5（2026-06-10 从2.2提升）
- ADHESION_RANGE = 20.0（2026-06-10 从15.0扩大）
- 贴墙态 kTangent = 0.55（原0.15）
- 贴墙态 maxLimitDist = 10.0（原6.8）

## 编译命令
cd /d/workspace/Venom && pio run

## 设计文档
贴合感设计方向.png — 6种贴墙形态参考图
