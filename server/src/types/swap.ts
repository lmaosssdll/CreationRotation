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

    swap(ending?: boolean, reason?: string): void
    addLevel(level: LevelData, accId: number): void
    checkSwap(): void
    scheduleNextSwap(): void
    completeTurn(): void
    removePlayer(userID: number): void
    clearTurnTimeout(): void
    unscheduleNextSwap(): void
}

export class Swap {
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
    timeout!: NodeJS.Timeout
    turnTimeout!: NodeJS.Timeout

    constructor(lobbyCode: string, state: ServerState) {
        this.lobbyCode = lobbyCode
        this.lobby = state.lobbies[lobbyCode]
        this.currentTurn = 0
        this.serverState = state

        this.totalTurns = getLength(this.lobby.accounts) * this.lobby.settings.turns

        this.levels = []

        this.swapEnded = false
        this.currentlySwapping = false
        this.isSwapEnding = false
        this.closeReason = ""

        // initialize swap order
        this.swapOrder = this.lobby.accounts.map(acc => acc.accountID)
    }

    swap(ending: boolean = false, reason: string = "") {
        this.levels = []
        this.currentTurn++
        this.currentlySwapping = true
        this.isSwapEnding = ending
        this.closeReason = reason

        // Recalculate swapOrder based on current lobby members
        const currentLobbyAccounts = this.serverState.lobbies[this.lobbyCode].accounts
        this.swapOrder = currentLobbyAccounts.map(acc => acc.accountID)

        log.debug(`[Swap] Turn ${this.currentTurn}/${this.totalTurns} started for lobby ${this.lobbyCode}. Swap order: ${this.swapOrder}`)

        // Set turn timeout
        this.scheduleNextSwap()

        emitToLobby(this.serverState, this.lobbyCode, Packet.TimeToSwapPacket, {})
    }

    addLevel(level: LevelData, accId: number) {
        if (!this.currentlySwapping) {
            log.warn(`[Swap] Received late or duplicate level from ${accId} when not swapping. Ignoring.`)
            return
        }

        // Validate level data is not empty
        if (!level || !level.levelString || level.levelString.trim() === "" ||
            !level.levelName || level.levelName.trim() === "") {
            log.warn(`[Swap] Player ${accId} sent empty or invalid level data. Ignoring.`)
            return
        }

        const idx = this.swapOrder.indexOf(accId)
        if (idx === -1) {
            log.error(`[Swap] Player ${accId} is not in the swapOrder list!`)
            return
        }

        const targetAccId = parseInt(offsetArray(this.swapOrder, 1)[idx] as string)

        // Prevent duplicate submissions messing up the array length
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
        const currentSockets = this.serverState.sockets[this.lobbyCode]

        // Update swapOrder to only include players still in the lobby
        this.swapOrder = this.swapOrder.filter(accID =>
            currentLobbyAccounts.some(acc => acc.accountID === accID)
        )

        // Get list of connected players
        const connectedAccountIDs = Object.values(currentSockets || {}).length > 0
            ? Object.keys(currentSockets).map(Number)
            : []

        // Check if all CONNECTED players have submitted their levels
        let allReady = true
        for (const acc of currentLobbyAccounts) {
            // Skip players who are no longer connected
            if (!connectedAccountIDs.includes(acc.accountID)) {
                continue
            }

            const hasLevelForThisPlayer = this.levels.some(l => l.accountID === acc.accountID)
            if (!hasLevelForThisPlayer) {
                allReady = false
                break
            }
        }

        if (!allReady) {
            return
        }

        log.info(`[Swap] All levels received for lobby ${this.lobbyCode}. Executing swap!`)
        this.completeTurn()
    }

    scheduleNextSwap() {
        if (this.swapEnded) return

        this.clearTurnTimeout()

        // Set timeout to complete the turn with whatever levels were submitted
        this.turnTimeout = setTimeout(() => {
            if (this.currentlySwapping) {
                if (this.levels.length > 0) {
                    log.warn(`[Swap] Turn ${this.currentTurn} timed out. Completing with ${this.levels.length} levels.`)
                    this.completeTurn()
                } else {
                    log.warn(`[Swap] Turn ${this.currentTurn} timed out with no levels. Skipping turn.`)
                    this.currentlySwapping = false
                    this.levels = []

                    if (this.currentTurn >= this.totalTurns) {
                        this.swapEnded = true
                        setTimeout(() => emitToLobby(this.serverState, this.lobbyCode, Packet.SwapEndedPacket, {}), 750)
                        return
                    }
                    this.scheduleNextSwap()
                }
            }
        }, this.lobby.settings.minutesPerTurn * 60_000)
    }

    completeTurn() {
        if (!this.currentlySwapping && this.levels.length === 0) return

        this.currentlySwapping = false
        log.info(`[Swap] Completing turn ${this.currentTurn} with ${this.levels.length} levels for lobby ${this.lobbyCode}.`)

        const currentLobbyAccounts = this.serverState.lobbies[this.lobbyCode].accounts

        // Only send levels to players who are actually still in the lobby
        const validLevels = this.levels.filter(l =>
            currentLobbyAccounts.some(acc => acc.accountID === l.accountID)
        )

        if (!this.isSwapEnding) {
            // Convert array to object format expected by client
            const levelsObj: { [key: number]: SwappedLevel } = {}
            for (const level of validLevels) {
                levelsObj[level.accountID] = level
            }

            emitToLobby(this.serverState, this.lobbyCode, Packet.ReceiveSwappedLevelPacket, { levels: levelsObj })

            this.levels = []

            if (this.currentTurn >= this.totalTurns) {
                this.swapEnded = true
                setTimeout(() => emitToLobby(this.serverState, this.lobbyCode, Packet.SwapEndedPacket, {}), 750)
                return
            }
            this.scheduleNextSwap()
        }
    }

    removePlayer(userID: number) {
        // Remove player from swapOrder
        const idx = this.swapOrder.indexOf(userID)
        if (idx !== -1) {
            this.swapOrder.splice(idx, 1)
            log.info(`[Swap] Removed player ${userID} from swap order for lobby ${this.lobbyCode}`)
        }

        // Also remove any levels assigned to this player
        this.levels = this.levels.filter(l => l.accountID !== userID)

        // Check if we can complete the turn now
        if (this.currentlySwapping) {
            this.checkSwap()
        }
    }

    clearTurnTimeout() {
        if (this.turnTimeout) {
            clearTimeout(this.turnTimeout)
        }
    }

    unscheduleNextSwap() {
        clearTimeout(this.timeout)
        this.clearTurnTimeout()
    }
}
