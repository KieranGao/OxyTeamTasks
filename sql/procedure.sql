SET NAMES utf8mb4;

-- ----------------------------
-- Procedure: reg_user
-- GateServer / UMSServer 注册流程依赖该存储过程（CALL reg_user(?,?,?,@result)）
-- 返回值：0 注册成功，1 用户名重复，2 邮箱重复，-1 异常
-- 新用户默认 role = 0（队员），status = 0（待审批）
-- ----------------------------
DROP PROCEDURE IF EXISTS `reg_user`;
DELIMITER $$
CREATE PROCEDURE `reg_user`(
    IN  p_username VARCHAR(255),
    IN  p_email    VARCHAR(255),
    IN  p_password VARCHAR(255),
    OUT p_result   INT
)
BEGIN
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        SET p_result = -1;
    END;

    IF EXISTS (SELECT 1 FROM `user` WHERE `username` = p_username) THEN
        SET p_result = 1;
    ELSEIF EXISTS (SELECT 1 FROM `user` WHERE `email` = p_email) THEN
        SET p_result = 2;
    ELSE
        INSERT INTO `user` (`username`, `email`, `password`, `role`, `status`)
        VALUES (p_username, p_email, p_password, 0, 0);
        SET p_result = 0;
    END IF;
END$$
DELIMITER ;
