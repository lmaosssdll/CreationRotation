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

        setInterval(() => {
            if (this.accountsToAuth.length <= 0) return
            this.updateMessagesCache()
        }, 4500)
    }

    // ✅ Определяет, слать запрос через CF Worker или напрямую
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

        // Если в конфиге указан CF Worker — шлём через него
        const fetchUrl = cfWorkerUrl
            ? `${cfWorkerUrl}?path=${encodeURIComponent(url)}`
            : `${this.state.serverConfig.boomlingsUrl}/${url}`

        log.info(`[boomlings] ${cfWorkerUrl ? "via CF Worker" : "direct"} → ${url}`)

        return (await fetch(fetchUrl, {
            headers: {
                "User-Agent": "",
                "Content-Type": "application/x-www-form-urlencoded"
            },
            method,
            body: params
        })).text()
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
        const messagesStr = (
            await this.sendAuthenticatedBoomlingsReq("database/getGJMessages20.php", {})
        ).split("|")

        log.info("refreshing cache")

        this.cachedMessages = {}
        messagesStr.forEach((messageStr) => {
            const msgObj = parseKeyMap(messageStr)
            const msgID = parseInt(msgObj["1"])
            if (isNaN(msgID)) return // пропускаем битые записи
            this.cachedMessages[msgID] = {
                accountID: parseInt(msgObj["2"]),
                age: msgObj["7"],
                messageID: msgID,
                playerID: parseInt(msgObj["3"]),
                title: Buffer.from(msgObj["4"] ?? "", "base64").toString("ascii"),
                username: msgObj["6"]
            }
        })

        let outdatedMessages: number[] = []

        Object.values(this.cachedMessages).forEach(message => {
            this.accountsToAuth.forEach(async acc => {
                if (message.accountID !== acc.account.accountID) return

                if (message.title === this.state.verifyCodes[acc.account.accountID]) {
                    const token = await this.state.dbState.registerUser(acc.account)
                    sendPacket(acc.socket, Packet.ReceiveTokenPacket, { token })
                    outdatedMessages.push(message.messageID)
                }
            })
        })

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
