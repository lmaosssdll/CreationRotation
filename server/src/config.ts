import * as fs from "fs"
import * as path from "path"
import appRootPath from "app-root-path"

export type Config = {
    webhookUrl?: string
    boomlingsUrl: string
    botAccountID: number
    botAccountGJP2: string
    masterPassword: string
}

const configPath = path.join(appRootPath.path, "config.json")

export default function getConfig(): Config {
    // Если есть переменные окружения (Render) — используем их
    if (process.env.BOT_ACCOUNT_ID) {
        return {
            webhookUrl: process.env.WEBHOOK_URL ?? "",
            boomlingsUrl: process.env.BOOMLINGS_URL ?? "https://boomlings.com",
            botAccountID: Number(process.env.BOT_ACCOUNT_ID),
            botAccountGJP2: process.env.BOT_ACCOUNT_GJP2 ?? "",
            masterPassword: process.env.MASTER_PASSWORD ?? "PLEASE_CHANGE_THIS"
        }
    }

    // Иначе читаем config.json (локально)
    if (!fs.existsSync(configPath)) {
        const defaultConfig: Config = {
            webhookUrl: "",
            boomlingsUrl: "https://boomlings.com",
            botAccountID: 0,
            botAccountGJP2: "",
            masterPassword: "PLEASE_CHANGE_THIS"
        }
        fs.writeFileSync(configPath, JSON.stringify(defaultConfig, null, 4))
    }

    return JSON.parse(fs.readFileSync(configPath).toString()) as Config
}
