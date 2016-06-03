// -------------------------------------------------------------------------
//    @FileName				:    NFCMapModule.cpp
//    @Author               :    LvSheng.Huang
//    @Date                 :    2013-07-05
//    @Module               :    NFCMapModule
//    @Desc                 :
// -------------------------------------------------------------------------

#include "NFCMapModule.h"
#include "NFComm/NFCore/NFTimer.h"

/*

leave_msg--list-->gridid_leavemsg
|BigMapLeaveMsg|
|BigMapLeaveMsg|
|BigMapLeaveMsg|
|BigMapLeaveMsg|

war_msg--list-->gridid_war
|BigMapWarHistory|
|BigMapWarHistory|
|BigMapWarHistory|
|BigMapWarHistory|

base_info--map-->grid_base
|GridID1, GridBaseData|
|GridID2, GridBaseData|
|GridID3, GridBaseData|

hurting_info--map-->hurting_info
|GridID, hurterID|
|GridID, hurterID|
|GridID, hurterID|

king_war_info--kv-->gridid_info

*/

bool NFCMapModule::Init()
{
	m_pBigMapRedisModule = pPluginManager->GetModule<NFIBigMapRedisModule>("NFCBigMapRedisModule");
	m_pKernelModule = pPluginManager->GetModule<NFIKernelModule>("NFCKernelModule");
	m_pLogicClassModule = pPluginManager->GetModule<NFILogicClassModule>("NFCLogicClassModule");
	m_pElementInfoModule = pPluginManager->GetModule<NFIElementInfoModule>("NFCElementInfoModule");
	m_pGuildModule = pPluginManager->GetModule<NFIGuildModule>("NFCGuildModule");
	
    m_pGameServerNet_ServerModule = pPluginManager->GetModule<NFIGameServerNet_ServerModule>("NFCGameServerNet_ServerModule");

    return true;
}

bool NFCMapModule::Shut()
{
    return true;
}

bool NFCMapModule::Execute()
{
    return true;
}

bool NFCMapModule::AfterInit()
{

	if (!m_pGameServerNet_ServerModule->AddReciveCallBack(NFMsg::EGameMsgID::EGMI_REQ_BIG_MAP_INFO, this, &NFCMapModule::ReqBigMapsInfo)) { return false; }
	if (!m_pGameServerNet_ServerModule->AddReciveCallBack(NFMsg::EGameMsgID::EGMI_REQ_MAP_GRID_INFO, this, &NFCMapModule::ReqMapTitleInfo)) { return false; }

	if (!m_pGameServerNet_ServerModule->AddReciveCallBack(NFMsg::EGameMsgID::EGMI_REQ_HOLD_MAP_GRID, this, &NFCMapModule::ReqStation)) { return false; }
	if (!m_pGameServerNet_ServerModule->AddReciveCallBack(NFMsg::EGameMsgID::EGMI_REQ_GET_MAP_GRID_AWARD, this, &NFCMapModule::ReqGetMapAward)) { return false; }
	if (!m_pGameServerNet_ServerModule->AddReciveCallBack(NFMsg::EGameMsgID::EGMI_REQ_LEAVE_MSG_MAP_GRID, this, &NFCMapModule::ReqLeaveMsgToMap)) { return false; }

	if (!m_pGameServerNet_ServerModule->AddReciveCallBack(NFMsg::EGameMsgID::EGMI_REQ_MAP_GRID_HUNTING, this, &NFCMapModule::ReqMapHunting)) { return false; }
	if (!m_pGameServerNet_ServerModule->AddReciveCallBack(NFMsg::EGameMsgID::EGMI_REQ_MAP_GRID_KING_WAR, this, &NFCMapModule::ReqMapKingWar)) { return false; }

    return true;
}

void NFCMapModule::ReqBigMapsInfo(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqBigMapInfo);


	NF_SHARE_PTR<NFILogicClass> xLogicClass = m_pLogicClassModule->GetElement("Map");
	NFList<std::string>& xElementList = xLogicClass->GetConfigNameList();
	
	//find all title info
	NFMsg::AckBigMapInfo xAckBigMapInfo;

	std::string strID;
	for (xElementList.First(strID); !strID.empty(); xElementList.Next(strID))
	{
		NFMsg::BigMapGridBaseInfo* pBigMapGridBaseInfo = xAckBigMapInfo.add_grid_base_info();
		if (pBigMapGridBaseInfo)
		{
			m_pBigMapRedisModule->GetGridBaseInfo(strID, *pBigMapGridBaseInfo);
		}

		strID.clear();
	}

	m_pGameServerNet_ServerModule->SendMsgPBToGate(NFMsg::EGMI_ACK_BIG_MAP_INFO, xAckBigMapInfo, nPlayerID);
}

void NFCMapModule::ReqMapTitleInfo(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqBigMapGridInfo);
	
	NFMsg::AckBigMapGridInfo xAckBigMapGridInfo;

	int nTitleCount = xMsg.map_title_id_size();
	for (int i = 0; i < nTitleCount; ++i)
	{
		std::string strTitleID = xMsg.map_title_id(i);

		if (!m_pElementInfoModule->ExistElement(strTitleID))
		{
			continue;
		}

		NFMsg::BigMapGridBaseInfo xGridBaseInfo;
		if (!m_pBigMapRedisModule->GetGridBaseInfo(strTitleID, xGridBaseInfo))
		{
			continue;
		}

		std::vector<NFMsg::BigMapLeaveMsg> xLeaveMsgList;
		if (!m_pBigMapRedisModule->GetGridLeaveMsgInfo(strTitleID, xLeaveMsgList))
		{
			continue;
		}

		std::vector<NFMsg::BigMapWarHistory> xWarHistoryList;
		if (!m_pBigMapRedisModule->GetGridWarHistoryInfo(strTitleID, xWarHistoryList))
		{
			continue;
		}

		NFMsg::BigMapGridDetailInfo* pBigMapGridDetailInfo = xAckBigMapGridInfo.add_map_data();
		if (pBigMapGridDetailInfo)
		{
			pBigMapGridDetailInfo->mutable_grid_base_info()->CopyFrom(xGridBaseInfo);
		}

		std::vector<NFMsg::BigMapLeaveMsg>::iterator itLeaveMsg = xLeaveMsgList.begin();
		for (itLeaveMsg; itLeaveMsg != xLeaveMsgList.end(); ++itLeaveMsg)
		{
			NFMsg::BigMapLeaveMsg* pBigMapLeaveMsg = pBigMapGridDetailInfo->add_leave_msg();
			if (pBigMapLeaveMsg)
			{
				pBigMapLeaveMsg->CopyFrom(*itLeaveMsg);
			}
		}

		std::vector<NFMsg::BigMapWarHistory>::iterator itWar = xWarHistoryList.begin();
		for (itWar; itWar != xWarHistoryList.end(); ++itWar)
		{
			NFMsg::BigMapWarHistory* pBigMapWarHistory = pBigMapGridDetailInfo->add_war_history();
			if (pBigMapWarHistory)
			{
				pBigMapWarHistory->CopyFrom(*itWar);
			}
		}
	}

	m_pGameServerNet_ServerModule->SendMsgPBToGate(NFMsg::EGMI_ACK_MAP_GRID_INFO, xAckBigMapGridInfo, nPlayerID);
}

void NFCMapModule::ReqStation(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqHoldMapGrid);

	if (!m_pElementInfoModule->ExistElement(xMsg.map_title_id()))
	{
		return;
	}


	NFMsg::BigMapGridBaseInfo xGridBaseInfo;
	if (!m_pBigMapRedisModule->GetGridBaseInfo(xMsg.map_title_id(), xGridBaseInfo))
	{
		return;
	}

	//get all guild information to set in grid base info
	//m_pGuildModule->GetGuildBaseInfo()


}

void NFCMapModule::ReqGetMapAward(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqGetMapAward);

	if (!m_pElementInfoModule->ExistElement(xMsg.map_title_id()))
	{
		return;
	}

	//get guild informatin and send mail to all members to send award item
}

void NFCMapModule::ReqLeaveMsgToMap(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqLeaveMapMsg);

	if (!m_pElementInfoModule->ExistElement(xMsg.map_title_id()))
	{
		return;
	}

	m_pBigMapRedisModule->AddGridLeaveMsgInfo(xMsg.map_title_id(), xMsg.leave_msg());
}

void NFCMapModule::ReqMapHunting(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqMapHunting);

	if (!m_pElementInfoModule->ExistElement(xMsg.map_title_id()))
	{
		return;
	}

	NFMsg::BigMapGridBaseInfo xGridBaseInfo;
	if (!m_pBigMapRedisModule->GetGridBaseInfo(xMsg.map_title_id(), xGridBaseInfo))
	{
		return;
	}

	xGridBaseInfo.set_hurting_time(0);
	xGridBaseInfo.mutable_hurter()->CopyFrom(NFINetModule::NFToPB(nPlayerID));

	m_pBigMapRedisModule->SetGridBaseInfo(xMsg.map_title_id(), xGridBaseInfo);

	//show msgbox
}

void NFCMapModule::ReqMapKingWar(const int nSockIndex, const int nMsgID, const char* msg, const uint32_t nLen)
{
	CLIENT_MSG_PROCESS(nSockIndex, nMsgID, msg, nLen, NFMsg::ReqMapKingWar);

	if (!m_pElementInfoModule->ExistElement(xMsg.map_title_id()))
	{
		return;
	}

	NFMsg::BigMapGridBaseInfo xGridBaseInfo;
	if (!m_pBigMapRedisModule->GetGridBaseInfo(xMsg.map_title_id(), xGridBaseInfo))
	{
		return;
	}

	xGridBaseInfo.set_kingwar_time(NFTime::GetNowTime());
	xGridBaseInfo.mutable_kingwarrer()->CopyFrom(NFINetModule::NFToPB(nPlayerID));

	m_pBigMapRedisModule->SetGridBaseInfo(xMsg.map_title_id(), xGridBaseInfo);

	//show msgbox
}
