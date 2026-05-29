/*
 Navicat Premium Data Transfer

 Source Server         : Ubuntu Linux
 Source Server Type    : MySQL
 Source Server Version : 80045 (8.0.45-0ubuntu0.22.04.1)
 Source Host           : 127.0.0.1:3306
 Source Schema         : oxytasks

 Target Server Type    : MySQL
 Target Server Version : 80045 (8.0.45-0ubuntu0.22.04.1)
 File Encoding         : 65001

 Date: 29/05/2026 21:04:36
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for task
-- ----------------------------
DROP TABLE IF EXISTS `task`;
CREATE TABLE `task`  (
  `id` int NOT NULL AUTO_INCREMENT,
  `uid` int NOT NULL COMMENT '创建者用户ID',
  `title` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT '任务标题',
  `description` text CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NULL COMMENT '任务描述',
  `status` tinyint NOT NULL DEFAULT 0 COMMENT '0=待处理 1=进行中 2=已完成',
  `priority` tinyint NOT NULL DEFAULT 3 COMMENT '1=高 2=中 3=低',
  `deadline` timestamp NULL DEFAULT NULL,
  `assigned_to` varchar(500) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL DEFAULT '',
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`) USING BTREE,
  INDEX `idx_task_uid`(`uid` ASC) USING BTREE,
  INDEX `idx_task_assigned_to`(`assigned_to` ASC) USING BTREE,
  INDEX `idx_task_status`(`status` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 14 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
