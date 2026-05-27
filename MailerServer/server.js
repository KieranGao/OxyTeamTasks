const grpc = require('@grpc/grpc-js');
const emailModule = require('./email');
const const_module = require('./const');
const { messageProto: message_proto } = require('./proto');

// SendMail: receives email + verification code from UMSServer, then sends email
async function SendMail(call, callback) {
    console.log("SendMail called, email:", call.request.email, "code:", call.request.code)
    try {
        const uniqueId = call.request.code;
        const text_str = 'Your verify code is ' + uniqueId + ' Please complete registration within three minutes';

        let mailOptions = {
            from: 'a2556469280@163.com',
            to: call.request.email,
            subject: 'Verify Code',
            text: text_str,
        };

        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send result:", send_res);

        callback(null, {
            error: const_module.Errors.Success
        });
    } catch (error) {
        console.log("SendMail error:", error);
        callback(null, {
            error: const_module.Errors.Exception
        });
    }
}

function sendHeartbeat() {
    const client = new message_proto.StatusService('127.0.0.1:50052', grpc.credentials.createInsecure());
    client.ServerHeartbeat({
        service: 'MailerServer',
        host: '127.0.0.1',
        port: '50051',
        timestamp: Math.floor(Date.now() / 1000)
    }, (err, resp) => {
        if (err) console.log('[Heartbeat] StatusServer unreachable:', err.message);
    });
}

function main() {
    var server = new grpc.Server();
    server.addService(message_proto.MailerService.service, { SendMail: SendMail });

    server.bindAsync('127.0.0.1:50051', grpc.ServerCredentials.createInsecure(), () => {
        console.log('MailerServer grpc started on :50051');
        // Start heartbeat to StatusServer
        sendHeartbeat();
        setInterval(sendHeartbeat, 10000);
    });
}

main();
