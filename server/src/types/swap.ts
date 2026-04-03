import { Lobby } from "./lobby"
import {
    emitToLobby,
    getLength,
    offsetArray
} from "@/utils"
import { Packet } from "./packet"
import { ServerState } from "./state"
import log from "@/logging"

export type AccsWithIdx = Array<{ index: number, accID: number }>

export type LevelData = {
    levelName: string
    songID: number
    songIDs: string
    levelString: string
}

export type SwappedLevel = {
    level: LevelData,
    accountID: number
}

export interface Swap {
    lobbyCode: string
    lobby: Lobby
    currentTurn: number

    levels: SwappedLevel[]

    totalTurns: number
    swapEnded: boolean
    swapOrder: number[]
    currentlySwapping: boolean
    isSwapEnding: boolean
    closeReason: string

    serverState: ServerState

    timeout: NodeJS.Timeout
}

const DUMMY_LEVEL_DATA: SwappedLevel = {
    level: {
        levelName: "THIS IS DUMMY DATA I REPEAT THIS IS DUMMY DATA",
        levelString: "THIS IS DUMMY DATA I REPEAT THIS IS DUMMY DATA",
        songID: 6942042069,
        songIDs: "THIS IS DUMMY DATA I REPEAT THIS IS DUMMY DATA"
    },
    accountID: 6942042069
}

export class Swap {
    constructor(lobbyCode: string, state: ServerState) {
        this.lobbyCode = lobbyCode
        this.lobby = state.lobbies[lobbyCode]
        this.currentTurn = 0
        this.serverState = state

        this.totalTurns = getLength(this.lobby.accounts) * this.lobby.settings.turns

        this.levels = []

        this.currentlySwapping = false
        this.isSwapEnding = false

        // initialize swap order
        this.swapOrder = this.lobby.accounts.map(acc => acc.accountID)
    }
    
    swap(ending: boolean = false, reason: string = "") {
        this.levels = []
        this.currentTurn++
        this.currentlySwapping = true
        this.isSwapEnding = ending
        this.closeReason = reason
        log.debug(`[Swap] Turn ${this.currentTurn}/${this.totalTurns} started for lobby ${this.lobbyCode}. Swap order: ${this.swapOrder}`)
        emitToLobby(this.serverState, this.lobbyCode, Packet.TimeToSwapPacket, {})
    }

    addLevel(level: LevelData, accId: number) {
        if (!this.currentlySwapping) {
            log.warn(`[Swap] Received late or duplicate level from ${accId} when not swapping. Ignoring.`)
            return
        }

        const idx = this.swapOrder.indexOf(accId)
        if (idx === -1) {
            log.error(`[Swap] Player ${accId} is not in the swapOrder list!`)
            return
        }

        const targetAccId = parseInt(offsetArray(this.swapOrder, 1)[idx] as string)

        // FIX: Prevent duplicate submissions messing up the array length.
        // If they already submitted, overwrite it. Otherwise, add it.
        const existingIdx = this.levels.findIndex(l => l.accountID === targetAccId)
        
        if (existingIdx !== -1) {
            log.warn(`[Swap] Player ${accId} sent level data AGAIN. Overwriting previous submission.`)
            this.levels[existingIdx] = { accountID: targetAccId, level }
        } else {
            this.levels.push({ accountID: targetAccId, level })
        }

        log.info(`[Swap] Received level from ${accId} (Size: ${level.levelString.length} bytes). Forwarding to player ${targetAccId}. (${this.levels.length}/${this.swapOrder.length})`)

        this.checkSwap()
    }

    checkSwap() {
        if (!this.currentlySwapping) return

        const currentLobbyAccounts = this.serverState.lobbies[this.lobbyCode].accounts

        // FIX: Instead of checking array length (which breaks on duplicates or disconnects),
        // we strictly check: Does EVERY player currently in the lobby have a level assigned to them?
        let allReady = true
        for (const acc of currentLobbyAccounts) {
            const hasLevelForThisPlayer = this.levels.some(l => l.accountID === acc.accountID)
            if (!hasLevelForThisPlayer) {
                allReady = false
                break
            }
        }

        if (!allReady) {
            // We are still waiting for someone to finish uploading their level
            return
        }

        // Everyone has submitted their level! We can proceed.
        this.currentlySwapping = false
        log.info(`[Swap] All levels received for lobby ${this.lobbyCode}. Executing swap!`)

        // Clean up: only send levels to players who are actually still in the lobby
        const validLevels = this.levels.filter(l => 
            currentLobbyAccounts.some(acc => acc.accountID === l.accountID)
        )

        if (!this.isSwapEnding) {
            emitToLobby(this.serverState, this.lobbyCode, Packet.ReceiveSwappedLevelPacket, { levels: validLevels })

            this.levels = []

            if (this.currentTurn >= this.totalTurns) {
                this.swapEnded = true
                setTimeout(() => emitToLobby(this.serverState, this.lobbyCode, Packet.SwapEndedPacket, {}), 750) // 0.75 seconds
                return
            }
            this.scheduleNextSwap()
        }
    }

    scheduleNextSwap() {
        if (this.swapEnded) return
        this.timeout = setTimeout(() => {
            this.swap()
        }, this.lobby.settings.minutesPerTurn * 60_000) 
    }

    unscheduleNextSwap() {
        clearTimeout(this.timeout)
    }
}
