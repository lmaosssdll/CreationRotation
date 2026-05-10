import { Handlers } from "@/types/handlers"
import {
    sendError,
    emitToLobby
} from "@/utils"
import { Swap, AccsWithIdx } from "@/types/swap"
import { Packet } from "@/types/packet"
import log from "@/logging"

const swapHandlers: Handlers = {
    2007: (socket, _, data, state) => { // StartSwapPacket
        const { currentLobbyCode: lobbyCode, account } = data

        if (!lobbyCode || !account) return

        if (!Object.keys(state.lobbies).includes(lobbyCode)) return
        if (state.lobbies[lobbyCode].settings.owner.userID != account.userID) {
            sendError(socket, "you are not the owner of this lobby")
            return
        }
        if (state.lobbies[lobbyCode].accounts.length <= 1) {
            sendError(socket, "you are the only person in the lobby, cannot start level swap")
            return
        }

        emitToLobby(state, lobbyCode, Packet.SwapStartedPacket, {})

        state.swaps[lobbyCode] = new Swap(lobbyCode, state)
        state.swaps[lobbyCode].scheduleNextSwap()
    },
    
    3001: (socket, args, data, state) => { // SendLevelPacket
        const { currentLobbyCode: code, account } = data
        if (!code) {
            sendError(socket, "you are not in a lobby")
            return
        }
        
        // Validate the player is in the swap
        if (!state.swaps[code]) {
            sendError(socket, "swap has not been started in this lobby")
            return
        }
        
        const accId = account?.accountID || 0
        log.info(`[Swap] Received level from player ${accId} in lobby ${code}`)
        state.swaps[code].addLevel(args.level, accId)
    }
}

export default swapHandlers
