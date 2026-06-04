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

 Date: 29/05/2026 21:04:49
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for task_assignments
-- ----------------------------
DROP TABLE IF EXISTS `task_assignments`;
CREATE TABLE `task_assignments`  (
  `task_id` int NOT NULL,
  `assignee_uid` int NOT NULL,
  `status` int NULL DEFAULT 0,
  `updated_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`task_id`, `assignee_uid`) USING BTREE
) ENGINE = InnoDB CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
