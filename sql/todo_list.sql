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

 Date: 29/05/2026 21:05:01
*/

SET NAMES utf8mb4;
SET FOREIGN_KEY_CHECKS = 0;

-- ----------------------------
-- Table structure for todo_list
-- ----------------------------
DROP TABLE IF EXISTS `todo_list`;
CREATE TABLE `todo_list`  (
  `id` int NOT NULL AUTO_INCREMENT COMMENT '代办事务唯一ID',
  `uid` int NOT NULL COMMENT '用户ID',
  `content` varchar(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci NOT NULL COMMENT '代办内容',
  `priority` tinyint NOT NULL DEFAULT 3 COMMENT '优先级',
  `deadline` timestamp NULL DEFAULT NULL COMMENT '截止时间',
  `is_finished` tinyint NOT NULL DEFAULT 2 COMMENT '是否已经完成',
  PRIMARY KEY (`id`) USING BTREE,
  INDEX `idx_todo_uid_priority_deadline`(`uid` ASC, `priority` DESC, `deadline` ASC) USING BTREE COMMENT '用于快速查找todolist显示信息\r\n'
) ENGINE = InnoDB AUTO_INCREMENT = 4 CHARACTER SET = utf8mb4 COLLATE = utf8mb4_0900_ai_ci ROW_FORMAT = Dynamic;

SET FOREIGN_KEY_CHECKS = 1;
