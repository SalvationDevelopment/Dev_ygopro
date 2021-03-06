#include "replay_mode.h"
#include "duelclient.h"
#include "game.h"
#include "../ocgcore/duel.h"
#include "../ocgcore/field.h"
#include "../ocgcore/mtrandom.h"

namespace ygo {

long ReplayMode::pduel = 0;
Replay ReplayMode::cur_replay;
bool ReplayMode::is_continuing = true;
bool ReplayMode::is_closing = false;
bool ReplayMode::is_pausing = false;
bool ReplayMode::is_paused = false;
bool ReplayMode::is_swaping = false;
bool ReplayMode::is_restarting = false;
bool ReplayMode::exit_pending = false;
int ReplayMode::skip_turn = 0;
int ReplayMode::current_step = 0;
int ReplayMode::skip_step = 0;

bool ReplayMode::StartReplay(int skipturn) {
	skip_turn = skipturn;
	Thread::NewThread(ReplayThread, 0);
	return true;
}
void ReplayMode::StopReplay(bool is_exiting) {
	is_pausing = false;
	is_continuing = false;
	is_closing = is_exiting;
	exit_pending = true;
	mainGame->actionSignal.Set();
}
void ReplayMode::SwapField() {
	if(is_paused)
		mainGame->dField.ReplaySwap();
	else
		is_swaping = true;
}
void ReplayMode::Pause(bool is_pause, bool is_step) {
	if(is_pause)
		is_pausing = true;
	else {
		if(!is_step)
			is_pausing = false;
		mainGame->actionSignal.Set();
	}
}
bool ReplayMode::ReadReplayResponse() {
	unsigned char resp[64];
	bool result = cur_replay.ReadNextResponse(resp);
	if(result)
		set_responseb(pduel, resp);
	return result;
}
int ReplayMode::ReplayThread(void* param) {
	const ReplayHeader& rh = cur_replay.pheader;
	mainGame->dInfo.isFirst = true;
	mtrandom rnd;
	int seed = rh.seed;
	rnd.reset(seed);
	if(rh.flag & REPLAY_TAG) {
		cur_replay.ReadData(mainGame->dInfo.hostname, 40);
		cur_replay.ReadData(mainGame->dInfo.hostname_tag, 40);
		cur_replay.ReadData(mainGame->dInfo.clientname_tag, 40);
		cur_replay.ReadData(mainGame->dInfo.clientname, 40);
		mainGame->dInfo.isTag = true;
		mainGame->dInfo.tag_player[0] = false;
		mainGame->dInfo.tag_player[1] = false;
	} else {
		cur_replay.ReadData(mainGame->dInfo.hostname, 40);
		cur_replay.ReadData(mainGame->dInfo.clientname, 40);
	}
	set_card_reader((card_reader)DataManager::CardReader);
	set_message_handler((message_handler)MessageHandler);
	pduel = create_duel(rnd.rand());
	int start_lp = cur_replay.ReadInt32();
	int start_hand = cur_replay.ReadInt32();
	int draw_count = cur_replay.ReadInt32();
	int opt = cur_replay.ReadInt32();
	set_player_info(pduel, 0, start_lp, start_hand, draw_count);
	set_player_info(pduel, 1, start_lp, start_hand, draw_count);
	mainGame->dInfo.lp[0] = start_lp;
	mainGame->dInfo.lp[1] = start_lp;
	myswprintf(mainGame->dInfo.strLP[0], L"%d", mainGame->dInfo.lp[0]);
	myswprintf(mainGame->dInfo.strLP[1], L"%d", mainGame->dInfo.lp[1]);
	mainGame->dInfo.turn = 0;
	if(!(opt & DUEL_TAG_MODE)) {
		int main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		int extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(0, main, extra);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(1, main, extra);
	} else {
		int main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		int extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(0, main, extra);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 0, LOCATION_DECK);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 0, LOCATION_EXTRA);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(1, main, extra);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 1, LOCATION_DECK);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 1, LOCATION_EXTRA);
	}
	start_duel(pduel, opt);
	ReplayRefreshDeck(0);
	ReplayRefreshDeck(1);
	ReplayRefreshExtra(0);
	ReplayRefreshExtra(1);
	mainGame->dInfo.isStarted = true;
	mainGame->dInfo.isReplay = true;
	char engineBuffer[0x1000];
	is_continuing = true;
	exit_pending = false;
	current_step = 0;
	skip_step = 0;
	if(skip_turn < 0)
		skip_turn = 0;
	if(skip_turn) {
		mainGame->dInfo.isReplaySkiping = true;
		mainGame->gMutex.Lock();
	} else
		mainGame->dInfo.isReplaySkiping = false;
	int len = 0;
	while (is_continuing && !exit_pending) {
		if (!mainGame->dInfo.isYRP2) {
			int result = process(pduel);
			len = result & 0xffff;
			/*int flag = result >> 16;*/
			if (len > 0) {
				get_message(pduel, (byte*)engineBuffer);
				is_continuing = ReplayAnalyze(engineBuffer, len);
			}
		}
		else {
			len = cur_replay.ReadInt16();
			if (len > 0) {
				char *msg = new char[len];
				cur_replay.ReadData(msg, len);
				is_continuing = ReplayAnalyze(msg, len);
			}
		}
	}
	if(mainGame->dInfo.isReplaySkiping) {
		mainGame->dInfo.isReplaySkiping = false;
		mainGame->dField.RefreshAllCards();
		mainGame->gMutex.Unlock();
	}
	end_duel(pduel);
	if(!is_closing) {
		mainGame->actionSignal.Reset();
		mainGame->gMutex.Lock();
		mainGame->stMessage->setText(dataManager.GetSysString(1501));
		if(mainGame->wCardSelect->isVisible())
			mainGame->HideElement(mainGame->wCardSelect);
		mainGame->PopupElement(mainGame->wMessage);
		mainGame->gMutex.Unlock();
		mainGame->actionSignal.Wait();
		mainGame->gMutex.Lock();
		mainGame->dInfo.isStarted = false;
		mainGame->dInfo.isReplay = false;
		mainGame->gMutex.Unlock();
		mainGame->closeDoneSignal.Reset();
		mainGame->closeSignal.Set();
		mainGame->closeDoneSignal.Wait();
		mainGame->gMutex.Lock();
		mainGame->wReplayList.Show();
		mainGame->gMutex.Unlock();
	}
	return 0;
}
void ReplayMode::Restart(bool refresh) {
	end_duel(pduel);
	mainGame->dInfo.isStarted = false;
	mainGame->dField.panel = 0;
	mainGame->dField.hovered_card = 0;
	mainGame->dField.clicked_card = 0;
	mainGame->dField.Clear();
	//mainGame->device->setEventReceiver(&mainGame->dField);
	cur_replay.Rewind();
	const ReplayHeader& rh = cur_replay.pheader;
	//mainGame->dInfo.isFirst = true;
	mtrandom rnd;
	int seed = rh.seed;
	rnd.reset(seed);
	if(rh.flag & REPLAY_TAG) {
		cur_replay.ReadData(mainGame->dInfo.hostname, 40);
		cur_replay.ReadData(mainGame->dInfo.hostname_tag, 40);
		cur_replay.ReadData(mainGame->dInfo.clientname_tag, 40);
		cur_replay.ReadData(mainGame->dInfo.clientname, 40);
		mainGame->dInfo.isTag = true;
		mainGame->dInfo.tag_player[0] = false;
		mainGame->dInfo.tag_player[1] = false;
	} else {
		cur_replay.ReadData(mainGame->dInfo.hostname, 40);
		cur_replay.ReadData(mainGame->dInfo.clientname, 40);
	}
	//set_card_reader((card_reader)DataManager::CardReader);
	//set_message_handler((message_handler)MessageHandler);
	pduel = create_duel(rnd.rand());
	int start_lp = cur_replay.ReadInt32();
	int start_hand = cur_replay.ReadInt32();
	int draw_count = cur_replay.ReadInt32();
	int opt = cur_replay.ReadInt32();
	set_player_info(pduel, 0, start_lp, start_hand, draw_count);
	set_player_info(pduel, 1, start_lp, start_hand, draw_count);
	mainGame->dInfo.lp[0] = start_lp;
	mainGame->dInfo.lp[1] = start_lp;
	myswprintf(mainGame->dInfo.strLP[0], L"%d", mainGame->dInfo.lp[0]);
	myswprintf(mainGame->dInfo.strLP[1], L"%d", mainGame->dInfo.lp[1]);
	mainGame->dInfo.turn = 0;
	if(!(opt & DUEL_TAG_MODE)) {
		int main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		int extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(0, main, extra);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(1, main, extra);
	} else {
		int main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		int extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 0, 0, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(0, main, extra);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 0, LOCATION_DECK);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 0, LOCATION_EXTRA);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_DECK, 0, POS_FACEDOWN_DEFENSE);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_card(pduel, cur_replay.ReadInt32(), 1, 1, LOCATION_EXTRA, 0, POS_FACEDOWN_DEFENSE);
		mainGame->dField.Initial(1, main, extra);
		main = cur_replay.ReadInt32();
		for(int i = 0; i < main; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 1, LOCATION_DECK);
		extra = cur_replay.ReadInt32();
		for(int i = 0; i < extra; ++i)
			new_tag_card(pduel, cur_replay.ReadInt32(), 1, LOCATION_EXTRA);
	}
	start_duel(pduel, opt);
	if(refresh) {
		mainGame->dField.RefreshAllCards();
		mainGame->dInfo.isStarted = true;
		//mainGame->dInfo.isReplay = true;
	}
	skip_turn = 0;
	is_restarting = true;
}
void ReplayMode::Undo() {
	Restart(false);
	skip_step = current_step - 1;
	if(skip_step < 0)
		skip_step = 0;
	current_step = 0;
	if(skip_step) {
		mainGame->dInfo.isReplaySkiping = true;
		mainGame->gMutex.Lock();
		Pause(false, false);
	} else
		mainGame->dInfo.isReplaySkiping = false;
}
bool ReplayMode::ReplayAnalyze(char* msg, unsigned int len) {
	char* pbuf = msg;
	int player, count;
	is_restarting = false;
	while (pbuf - msg < (int)len) {
		if(is_closing)
			return false;
		if(is_restarting) {
			is_restarting = false;
			return true;
		}
		if(is_swaping) {
			mainGame->gMutex.Lock();
			mainGame->dField.ReplaySwap();
			mainGame->gMutex.Unlock();
			is_swaping = false;
		}
		char* offset = pbuf;
		bool pauseable = true;
		mainGame->dInfo.curMsg = BufferIO::ReadUInt8(pbuf);
		switch (mainGame->dInfo.curMsg) {
		case MSG_RETRY: {
			if(mainGame->dInfo.isReplaySkiping) {
				mainGame->dInfo.isReplaySkiping = false;
				mainGame->dField.RefreshAllCards();
				mainGame->gMutex.Unlock();
			}
			mainGame->gMutex.Lock();
			mainGame->stMessage->setText(L"Error occurs.");
			mainGame->PopupElement(mainGame->wMessage);
			mainGame->gMutex.Unlock();
			mainGame->actionSignal.Reset();
			mainGame->actionSignal.Wait();
			return false;
		}
		case MSG_HINT: {
			pbuf += 6;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_WIN: {
			if(mainGame->dInfo.isReplaySkiping) {
				mainGame->dInfo.isReplaySkiping = false;
				mainGame->dField.RefreshAllCards();
				mainGame->gMutex.Unlock();
			}
			if (mainGame->dInfo.isYRP2)
				return false;
			pbuf += 2;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			return false;
		}
		case MSG_SELECT_BATTLECMD: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 11;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 8 + 2;
			ReplayRefresh();
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_IDLECMD: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 11 + 3;
			ReplayRefresh();
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_EFFECTYN: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 8;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_YESNO: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 4;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_OPTION: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_CARD:
		case MSG_SELECT_TRIBUTE: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 3;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 8;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_CHAIN: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += 10 + count * 13;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_PLACE:
		case MSG_SELECT_DISFIELD: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_POSITION: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_COUNTER: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 3;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 8;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SELECT_SUM: {
			pbuf++;
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 6;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 11;
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 11;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_SORT_CARD:
		case MSG_SORT_CHAIN: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_CONFIRM_DECKTOP: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CONFIRM_CARDS: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 7;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SHUFFLE_DECK: {
			player = BufferIO::ReadInt8(pbuf);
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefreshDeck(player);
			break;
		}
		case MSG_SHUFFLE_HAND: {
			/*int oplayer = */BufferIO::ReadInt8(pbuf);
			int count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_REFRESH_DECK: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SWAP_GRAVE_DECK: {
			player = BufferIO::ReadInt8(pbuf);
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefreshGrave(player);
			break;
		}
		case MSG_REVERSE_DECK: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_DECK_TOP: {
			pbuf += 6;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SHUFFLE_SET_CARD: {
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_NEW_TURN: {
			if(skip_turn) {
				skip_turn--;
				if(skip_turn == 0) {
					mainGame->dInfo.isReplaySkiping = false;
					mainGame->dField.RefreshAllCards();
					mainGame->gMutex.Unlock();
				}
			}
			player = BufferIO::ReadInt8(pbuf);
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_NEW_PHASE: {
			pbuf += 2;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			break;
		}
		case MSG_MOVE: {
			int pc = pbuf[4];
			int pl = pbuf[5];
			/*int ps = pbuf[6];*/
			/*int pp = pbuf[7];*/
			int cc = pbuf[8];
			int cl = pbuf[9];
			int cs = pbuf[10];
			/*int cp = pbuf[11];*/
			pbuf += 16;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			if (mainGame->dInfo.isYRP2)
				return true;
			if(cl && !(cl & 0x80) && (pl != cl || pc != cc))
				ReplayRefreshSingle(cc, cl, cs);
			break;
		}
		case MSG_START: {
			mainGame->showcardcode = 11;
			mainGame->showcarddif = 30;
			mainGame->showcardp = 0;
			mainGame->showcard = 101;
			mainGame->WaitFrameSignal(40);
			mainGame->showcard = 0;
			mainGame->gMutex.Lock();
			int playertype = BufferIO::ReadInt8(pbuf);
			mainGame->dInfo.isFirst = (playertype & 0xf) ? false : true;
			if (playertype & 0xf0)
				mainGame->dInfo.player_type = 7;
			if (mainGame->dInfo.isTag) {
				if (mainGame->dInfo.isFirst)
					mainGame->dInfo.tag_player[1] = true;
				else
					mainGame->dInfo.tag_player[0] = true;
			}
			mainGame->dInfo.lp[mainGame->LocalPlayer(0)] = BufferIO::ReadInt32(pbuf);
			mainGame->dInfo.lp[mainGame->LocalPlayer(1)] = BufferIO::ReadInt32(pbuf);
			myswprintf(mainGame->dInfo.strLP[0], L"%d", mainGame->dInfo.lp[0]);
			myswprintf(mainGame->dInfo.strLP[1], L"%d", mainGame->dInfo.lp[1]);
			int deckc = BufferIO::ReadInt16(pbuf);
			int extrac = BufferIO::ReadInt16(pbuf);
			mainGame->dField.Initial(mainGame->LocalPlayer(0), deckc, extrac);
			deckc = BufferIO::ReadInt16(pbuf);
			extrac = BufferIO::ReadInt16(pbuf);
			mainGame->dField.Initial(mainGame->LocalPlayer(1), deckc, extrac);
			mainGame->dInfo.turn = 0;
			mainGame->dInfo.strTurn[0] = 0;
			mainGame->dInfo.is_shuffling = false;
			mainGame->gMutex.Unlock();
			return true;
		}
		case MSG_UPDATE_DATA: {
			int player = mainGame->LocalPlayer(BufferIO::ReadInt8(pbuf));
			int location = BufferIO::ReadInt8(pbuf);
			mainGame->gMutex.Lock();
			mainGame->dField.UpdateFieldCard(player, location, pbuf);
			mainGame->gMutex.Unlock();
			return true;
		}
		case MSG_UPDATE_CARD: {
			int player = mainGame->LocalPlayer(BufferIO::ReadInt8(pbuf));
			int loc = BufferIO::ReadInt8(pbuf);
			int seq = BufferIO::ReadInt8(pbuf);
			mainGame->gMutex.Lock();
			mainGame->dField.UpdateCard(player, loc, seq, pbuf);
			mainGame->gMutex.Unlock();
			return true;
		}
		case MSG_POS_CHANGE: {
			pbuf += 9;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_SET: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_SWAP: {
			pbuf += 16;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_FIELD_DISABLED: {
			pbuf += 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_SUMMONING: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_SUMMONED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			break;
		}
		case MSG_SPSUMMONING: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_SPSUMMONED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			break;
		}
		case MSG_FLIPSUMMONING: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_FLIPSUMMONED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			break;
		}
		case MSG_CHAINING: {
			pbuf += 16;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CHAINED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			break;
		}
		case MSG_CHAIN_SOLVING: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_CHAIN_SOLVED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			pauseable = false;
			break;
		}
		case MSG_CHAIN_END: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			pauseable = false;
			break;
		}
		case MSG_CHAIN_NEGATED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CHAIN_DISABLED: {
			pbuf++;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_CARD_SELECTED:
		case MSG_RANDOM_SELECTED: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_BECOME_TARGET: {
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_DRAW: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count * 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_DAMAGE: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_RECOVER: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_EQUIP: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_LPUPDATE: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_UNEQUIP: {
			pbuf += 4;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_CARD_TARGET: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_CANCEL_TARGET: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_PAY_LPCOST: {
			pbuf += 5;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ADD_COUNTER: {
			pbuf += 7;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_REMOVE_COUNTER: {
			pbuf += 7;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ATTACK: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_BATTLE: {
			pbuf += 26;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_ATTACK_DISABLED: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			pauseable = false;
			break;
		}
		case MSG_DAMAGE_STEP_START: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			pauseable = false;
			break;
		}
		case MSG_DAMAGE_STEP_END: {
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefresh();
			pauseable = false;
			break;
		}
		case MSG_MISSED_EFFECT: {
			pbuf += 8;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_TOSS_COIN: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_TOSS_DICE: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += count;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_ANNOUNCE_RACE: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_ANNOUNCE_ATTRIB: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 5;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_ANNOUNCE_CARD: {
			player = BufferIO::ReadInt8(pbuf);
			pbuf += 4;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_ANNOUNCE_NUMBER: {
			player = BufferIO::ReadInt8(pbuf);
			count = BufferIO::ReadInt8(pbuf);
			pbuf += 4 * count;
			return mainGame->dInfo.isYRP2 ? true : ReadReplayResponse();
		}
		case MSG_CARD_HINT: {
			pbuf += 9;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_PLAYER_HINT: {
			pbuf += 6;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			break;
		}
		case MSG_MATCH_KILL: {
			pbuf += 4;
			break;
		}
		case MSG_TAG_SWAP: {
			player = pbuf[0];
			pbuf += pbuf[2] * 4 + pbuf[4] * 4 + 9;
			DuelClient::ClientAnalyze(offset, pbuf - offset);
			ReplayRefreshDeck(player);
			ReplayRefreshExtra(player);
			break;
		}
		default: {
			return mainGame->dInfo.isYRP2 ? true : false;
		}
		}
		if(pauseable) {
			if(skip_step) {
				skip_step--;
				if(skip_step == 0) {
					Pause(true, false);
					mainGame->dInfo.isStarted = true;
					mainGame->dInfo.isReplaySkiping = false;
					mainGame->dField.RefreshAllCards();
					mainGame->gMutex.Unlock();
				}
			}
			if(is_pausing) {
				is_paused = true;
				mainGame->actionSignal.Reset();
				mainGame->actionSignal.Wait();
				is_paused = false;
			}
			current_step++;
		}
	}
	return true;
}
void ReplayMode::ReplayRefresh(int flag) {
	if (mainGame->dInfo.isYRP2)
		return;
	unsigned char queryBuffer[0x2000];
	/*int len = */query_field_card(pduel, 0, LOCATION_MZONE, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(0), LOCATION_MZONE, (char*)queryBuffer);
	/*len = */query_field_card(pduel, 1, LOCATION_MZONE, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(1), LOCATION_MZONE, (char*)queryBuffer);
	/*len = */query_field_card(pduel, 0, LOCATION_SZONE, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(0), LOCATION_SZONE, (char*)queryBuffer);
	/*len = */query_field_card(pduel, 1, LOCATION_SZONE, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(1), LOCATION_SZONE, (char*)queryBuffer);
	/*len = */query_field_card(pduel, 0, LOCATION_HAND, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(0), LOCATION_HAND, (char*)queryBuffer);
	/*len = */query_field_card(pduel, 1, LOCATION_HAND, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(1), LOCATION_HAND, (char*)queryBuffer);
}
void  ReplayMode::ReplayRefreshHand(int player, int flag) {
	if (mainGame->dInfo.isYRP2)
		return;
	unsigned char queryBuffer[0x2000];
	/*int len = */query_field_card(pduel, player, LOCATION_HAND, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(player), LOCATION_HAND, (char*)queryBuffer);
}
void ReplayMode::ReplayRefreshGrave(int player, int flag) {
	if (mainGame->dInfo.isYRP2)
		return;
	unsigned char queryBuffer[0x2000];
	/*int len = */query_field_card(pduel, player, LOCATION_GRAVE, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(player), LOCATION_GRAVE, (char*)queryBuffer);
}
void ReplayMode::ReplayRefreshDeck(int player, int flag) {
	if (mainGame->dInfo.isYRP2)
		return;
	unsigned char queryBuffer[0x2000];
	/*int len = */query_field_card(pduel, player, LOCATION_DECK, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(player), LOCATION_DECK, (char*)queryBuffer);
}
void ReplayMode::ReplayRefreshExtra(int player, int flag) {
	if (mainGame->dInfo.isYRP2)
		return;
	unsigned char queryBuffer[0x2000];
	/*int len = */query_field_card(pduel, player, LOCATION_EXTRA, flag, queryBuffer, 0);
	mainGame->dField.UpdateFieldCard(mainGame->LocalPlayer(player), LOCATION_EXTRA, (char*)queryBuffer);
}
void ReplayMode::ReplayRefreshSingle(int player, int location, int sequence, int flag) {
	if (mainGame->dInfo.isYRP2)
		return;
	unsigned char queryBuffer[0x2000];
	/*int len = */query_card(pduel, player, location, sequence, flag, queryBuffer, 0);
	mainGame->dField.UpdateCard(mainGame->LocalPlayer(player), location, sequence, (char*)queryBuffer);
}
int ReplayMode::MessageHandler(long fduel, int type) {
	if(!enable_log)
		return 0;
	char msgbuf[1024];
	get_log_message(fduel, (byte*)msgbuf);
	if(enable_log == 1) {
		wchar_t wbuf[1024];
		BufferIO::DecodeUTF8(msgbuf, wbuf);
		mainGame->AddChatMsg(wbuf, 9);
	} else if(enable_log == 2) {
		FILE* fp = fopen("error.log", "at");
		if(!fp)
			return 0;
		fprintf(fp, "[Script error:] %s\n", msgbuf);
		fclose(fp);
	}
	return 0;
}

}
