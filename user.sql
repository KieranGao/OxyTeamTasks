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

 Date: 29/05/2026 21:05:12
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for user
-- ----------------------------
DROP TABLE IF EXISTS `user`;
CREATE TABLE `user`  (
  `uid` int NOT NULL AUTO_INCREMENT,
  `username` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `email` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `password` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL,
  `role` tinyint(1) NOT NULL COMMENT 'role = 0 为队员\r\nrole = 1 为队长\r\nrole = 2 为教练',
  `belong_captain_id` int NULL DEFAULT NULL COMMENT '如有队长，队长ID是什么',
  `status` tinyint(1) NOT NULL COMMENT '0 - 待审核 1 - 正常 2 - 已被禁用',
  `belong_team_id` int NULL DEFAULT NULL COMMENT '所属队伍ID',
  PRIMARY KEY (`uid`, `email`) USING BTREE,
  UNIQUE INDEX `uk_username`(`username` ASC) USING BTREE,
  UNIQUE INDEX `uk_email`(`email` ASC) USING BTREE
) ENGINE = InnoDB AUTO_INCREMENT = 6 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
