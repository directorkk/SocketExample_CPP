// Fill out your copyright notice in the Description page of Project Settings.

#include "pch.h"
#include "SocketHandlerBase.h"

SocketHandlerBase::SocketHandlerBase(WORD sockVersion)
{
	if (WSAStartup(sockVersion, &mWsaData) != 0) {
		WSACleanup();

		OutputDebugString(L"ERROR: Socket version is not supported");
		mFSuccess = false;

		return;
	}
	mFSuccess = true;
}

SocketHandlerBase::~SocketHandlerBase()
{
	if (mFSuccess) {
		WSACleanup();
	}
}
