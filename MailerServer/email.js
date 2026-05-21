const nodemailer = require('nodemailer');
const config_module = require("./config")

/**
 * 创建发送邮件的代理
 */

let transport = nodemailer.createTransport({
    host: 'smtp.163.com',
    port: 465,
    secure: true,
    auth: {
        user: config_module.email_user, // 发送方邮箱地址
        pass: config_module.email_pass // 邮箱授权码或者密码
    },
    tls: {
        rejectUnauthorized: false // 允许自签名证书
    },
    debug: false, // 启用调试模式
    logger: false // 启用日志记录
});

/**
 * 发送邮件的函数
 * @param {*} mailOptions_ 发送邮件的参数
 * @returns 
 */

function SendMail(mailOptions_){
    return new Promise(function(resolve, reject){
        transport.sendMail(mailOptions_, function(error, info){
            if (error) {
                console.log(error);
                reject(error);
            } else {
                console.log('The email was sent successfully: ' + info.response);
                resolve(info.response)
            }
        });
    })

}

// 测试SMTP连接
async function testConnection() {
    try {
        await transport.verify();
        console.log('SMTP server connected successfully!');
    } catch (error) {
        console.error('SMTP server connection failed:', error);
    }
}

// 导出模块
module.exports.SendMail = SendMail;
module.exports.testConnection = testConnection;

// 启动时测试连接
testConnection();