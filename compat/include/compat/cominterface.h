/**
 * @file compat/cominterface.h
 * @brief C++Builder の TComInterface<T> (utilcls.h) 互換シム
 *
 * usr_wic.cpp が WIC を扱うのに使っている COM スマートポインタ。実測された
 * 呼び出し形は 4 つだけ:
 *   TComInterface<IWICImagingFactory> factory;          // 既定構築は NULL
 *   CoCreateInstance(..., (LPVOID*)&factory)            // operator& が T** を返す
 *   factory->CreateStream(&stream)                      // operator-> と T**
 *   converter->Initialize(frame, ...)                   // T* への暗黙変換
 * スコープを抜けたら Release する。
 */
#ifndef NYANFI_COMPAT_COMINTERFACE_H
#define NYANFI_COMPAT_COMINTERFACE_H

#include "compat/config.h"

/**
 * @brief COM インタフェースの参照カウントを管理するスマートポインタ
 * @tparam T COM インタフェース型
 */
template <class T>
class TComInterface {
public:
	TComInterface() = default;

	/// 既存のインタフェースを受け取る (AddRef する)
	explicit TComInterface(T *itf, bool addRef = true) : itf_(itf)
	{
		if (itf_ && addRef) itf_->AddRef();
	}

	TComInterface(const TComInterface &src) : itf_(src.itf_)
	{
		if (itf_) itf_->AddRef();
	}

	TComInterface(TComInterface &&src) noexcept : itf_(src.itf_) { src.itf_ = nullptr; }

	~TComInterface() { Release(); }

	TComInterface &operator=(const TComInterface &src)
	{
		if (this != &src) {
			T *old = itf_;
			itf_ = src.itf_;
			if (itf_) itf_->AddRef();
			if (old) old->Release();
		}
		return *this;
	}

	TComInterface &operator=(T *itf)
	{
		Bind(itf);
		return *this;
	}

	/// 保持を差し替える (既定では AddRef しない = 呼び出し先が渡した参照を引き継ぐ)
	void Bind(T *itf, bool addRef = false)
	{
		if (itf_ == itf) return;
		T *old = itf_;
		itf_ = itf;
		if (itf_ && addRef) itf_->AddRef();
		if (old) old->Release();
	}

	void Release()
	{
		if (itf_) {
			itf_->Release();
			itf_ = nullptr;
		}
	}

	T *operator->() const { return itf_; }
	operator T *() const { return itf_; }
	T *get() const { return itf_; }
	bool IsBound() const { return itf_ != nullptr; }

	/**
	 * @brief 出力引数として渡すためのアドレスを返す
	 * @details `CoCreateInstance(..., (LPVOID*)&factory)` や
	 *          `factory->CreateStream(&stream)` の形で使う。C++Builder と同様、
	 *          既に保持しているものは先に解放する。
	 */
	T **operator&()
	{
		Release();
		return &itf_;
	}

private:
	T *itf_ = nullptr;
};

#endif  // NYANFI_COMPAT_COMINTERFACE_H
