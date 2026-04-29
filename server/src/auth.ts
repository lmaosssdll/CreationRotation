import { Account } from "./types/account"
import { ServerState } from "./types/state"
import WebSocket from "ws"
import { sendPacket } from "./utils"
import { Packet } from "./types/packet"
import log from "./logging"

export interface AuthManager {
    state: ServerState
    accountsToAuth: Array<{ socket: WebSocket, account: Account }>
    cachedMessages: { [key: number]: Message }
}

export type Message = {
    messageID: number
    accountID: number
    playerID: number
    title: string
    username: string
    age: string
}

// thanks prevter!
const parseKeyMap = (keyMap: string) => keyMap.split(":")
    .reduce((acc, key, index, array) => {
        if (index % 2 === 0) {
            acc[key] = array[index + 1];
        }
        return acc;
    }, ({} as { [key: string]: any }));

export class AuthManager {
    constructor(state: ServerState) {
        this.state = state
        this.accountsToAuth = []
        this.cachedMessages = {}

        setInterval(() => {
            if (this.accountsToAuth.length <= 0) return
            this.updateMessagesCache()
        }, 4500)
    }

    private async sendBoomlingsReq(
        url: string,
        data: { [key: string]: string },
        method: string = "POST"
    ) {
        const params = new URLSearchParams({
            secret: "Wmfd2893gb7",
            ...data
        })

        const cfWorkerUrl = this.state.serverConfig.cfWorkerUrl

        const fetchUrl = cfWorkerUrl
            ? `${cfWorkerUrl}?path=${encodeURIComponent(url)}`
            : `${this.state.serverConfig.boomlingsUrl}/${url}`

        log.info(`[boomlings] ${cfWorkerUrl ? "via CF Worker" : "direct"} → ${url}`)

        try {
            const response = await fetch(fetchUrl, {
                headers: {
                    "User-Agent": "",
                    "Content-Type": "application/x-www-form-urlencoded"
                },
                method,
                body: params.toString()
            })
            return await response.text()
        } catch (e: any) {
            log.error(`[boomlings] Error: ${e.message}`)
            return "-1"
        }
    }

    private async sendAuthenticatedBoomlingsReq(
        url: string,
        data: { [key: string]: string }
    ) {
        return await this.sendBoomlingsReq(url, {
            gjp2: this.state.serverConfig.botAccountGJP2,
            accountID: `${this.state.serverConfig.botAccountID}`,
            ...data
        })
    }

    async getMessages() {
        return this.cachedMessages
    }

    private async updateMessagesCache() {
        const rawResponse = await this.sendAuthenticatedBoomlingsReq("database/getGJMessages20.php", {})

        // Лог для проверки ответа от Роба через Cloudflare
        log.info(`[DEBUG] Raw messages: ${rawResponse}`)

        if (!rawResponse || rawResponse === "-1" || rawResponse.startsWith("<")) {
            log.warn("[auth] Invalid response from Boomlings. Check bot config.")
            return
        }

        const messagesStr = rawResponse.split("|")
        this.cachedMessages = {}

        messagesStr.forEach((messageStr) => {
            const msgObj = parseKeyMap(messageStr)
            const msgID = parseInt(msgObj["1"])
            if (isNaN(msgID)) return 
            
            this.cachedMessages[msgID] = {
                accountID: parseInt(msgObj["2"]),
                age: msgObj["7"],
                messageID: msgID,
                playerID: parseInt(msgObj["3"]),
                title: Buffer.from(msgObj["4"] ?? "", "base64").toString("ascii"),
                username: msgObj["6"]
            }
        })

        log.info("refreshing cache")

        let outdatedMessages: number[] = []

        // Используем for...of для асинхронности
        for (const acc of this.accountsToAuth) {
            const verifyCode = this.state.verifyCodes[acc.account.accountID]
            
            // Ищем сообщение по ID аккаунта и коду в заголовке
            const message = Object.values(this.cachedMessages).find(
                m => m.accountID === acc.account.accountID && m.title.trim() === verifyCode
            )

            if (message) {
                // ИСПОЛЬЗУЕМ message.username, так как в Account его может не быть
                log.info(`[auth] Account verified: ${message.username} (ID: ${message.accountID})`)
                
                const token = await this.state.dbState.registerUser(acc.account)
                sendPacket(acc.socket, Packet.ReceiveTokenPacket, { token })
                outdatedMessages.push(message.messageID)
            }
        }

        // Чистим список ожидающих только после завершения цикла
        this.accountsToAuth = []

        if (outdatedMessages.length > 0) {
            await this.sendAuthenticatedBoomlingsReq("database/deleteGJMessages20.php", {
                messages: outdatedMessages.join(",")
            })
        }
    }

    async sendMessage(toAccID: number, subject: string, body: string) {
        return await this.sendAuthenticatedBoomlingsReq("database/uploadGJMessage20.php", {
            toAccountID: toAccID.toString(),
            subject: this.urlsafeb64(subject),
            body: this.urlsafeb64(this.xor(body, "14251"))
        })
    }

    urlsafeb64(input: string) {
        return Buffer.from(input, "utf8").toString("base64")
    }

    xor(input: string, key: string) {
        let result = ""
        const encoder = new TextEncoder()
        for (let i = 0; i < input.length; i++) {
            let byte = encoder.encode(input[i])[0]
            let xkey = encoder.encode(key[i % key.length])[0]
            result += String.fromCharCode(byte ^ xkey)
        }
        return result
    }
}
