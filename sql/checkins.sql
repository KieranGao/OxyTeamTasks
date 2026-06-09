/*
 Navicat Premium Data Transfer

 Source Server         : Ubuntu Linux
 Source Server Type    : MySQL
 Source Server Version : 80046 (8.0.46-0ubuntu0.22.04.2)
 Source Host           : 127.0.0.1:3306
 Source Schema         : oxytasks

 Target Server Type    : MySQL
 Target Server Version : 80046 (8.0.46-0ubuntu0.22.04.2)
 File Encoding         : 65001

 Date: 09/06/2026 18:56:18
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for checkins
-- ----------------------------
DROP TABLE IF EXISTS `checkins`;
CREATE TABLE `checkins`  (
  `id` int NOT NULL AUTO_INCREMENT,
  `uid` int NOT NULL,
  `checkin_date` date NOT NULL,
  `created_at` timestamp NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`) USING BTREE,
  UNIQUE INDEX `uk_uid_date`(`uid` ASC, `checkin_date` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 18 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
