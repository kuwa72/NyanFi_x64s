/**
 * @file gui/image_viewer.h
 * @brief 画像ビューア (自前描画、gui/text_viewer.h と同じ作り)
 *
 * @details MainFrame の中に常駐し、開いていないときは Hide() しておく1面の
 * ビューア。VCL 版 (src/imgv_thread.cpp、781行 + src/MainFrm.cpp の
 * イメージビューア関連コード) はスレッドによる非同期デコード・見開き表示・
 * グレースケール/グリッド/回転・ヒストグラム・ルーペ・サムネイル一覧・
 * スライドショー・全画面表示など多数の機能を持つが、ここは issue #1 の
 * このタスクのスコープに合わせて「カーソル位置の画像を表示し、フィット/
 * ズーム/等倍と前後移動ができる」だけに単純化してある (推測・要検証)。
 *
 * 画像のデコードは gui/image_load.h (wx 非依存、WIC_load_image を使う) に
 * 任せ、ここでは受け取った RGB24 バッファを wxImage/wxBitmap に変換して
 * 表示するだけ。フィット/ズーム計算は wx 側でその都度 wxImage::Scale する
 * (デコードのやり直しではなく、既にデコード済みのフルサイズ RGB からの
 * 再スケールなので WIC を毎回呼ぶより軽い。推測・要検証の設計判断)。
 *
 * 色は gui/file_pane.cpp と同じく wxSystemSettings から取り、ライト/ダーク
 * モードに自動追従する (要件8。VCL 版の col_bgImage 既定値 clBlack をそのまま
 * 使わなかった点は意図的な変更。報告に明記)。
 */
#ifndef NYANFI_GUI_IMAGE_VIEWER_H
#define NYANFI_GUI_IMAGE_VIEWER_H

#include <functional>
#include <vector>

#include <wx/wx.h>

#include "gui/image_load.h"

/**
 * @brief 画像ビューア
 */
class ImageViewer : public wxWindow {
public:
	ImageViewer(wxWindow *parent, wxWindowID id);

	/**
	 * @brief ファイルを開く
	 * @details 読み込みに失敗しても画面自体は開いたまま (閉じない) にし、
	 * エラーメッセージをヘッダ下に表示する。VCL 版 (imgv_thread.cpp の
	 * DrawMessage/MsgStr) がエラー時もビューアを閉じず、次のファイルへの
	 * 移動を妨げないのと同じ考え方 (要件7)。呼び出し側 (MainFrame) は
	 * 常にこれを呼んだ後 Show(true) すればよく、成否で分岐する必要が無い
	 */
	void LoadFile(const UnicodeString &path);

	/// 閉じるキー (Q/ESC/ENTER) が押されたときに呼ぶコールバック
	void SetOnClose(std::function<void()> fn) { on_close_ = std::move(fn); }

	/**
	 * @brief 前後の画像へ移動するキー (Left/Right、推測のキー) が押されたときに
	 * 呼ぶコールバック
	 * @details direction は -1 (前) / +1 (次)。ファイルの一覧・並び順は
	 * FilePane が持っているため、実際の移動先の決定と LoadFile の呼び直しは
	 * 呼び出し側 (MainFrame) に委ねる
	 */
	void SetOnNavigate(std::function<void(int direction)> fn) { on_navigate_ = std::move(fn); }

	/**
	 * @brief キー入力を処理する
	 * @return true 処理済み (MainFrame 側の通常のキー処理へは回さない)
	 */
	bool HandleKey(wxKeyEvent &event);

private:
	void OnPaint(wxPaintEvent &event);
	void OnSize(wxSizeEvent &event);
	void OnMouseWheel(wxMouseEvent &event);
	void OnMiddleDown(wxMouseEvent &event);

	void ToggleFitted();        //!< フィット表示のON/OFF切替 (F、推測のキー)
	void SetEqualSize();        //!< 等倍(100%)表示にする (I:EqualSize相当。キー "0"、推測)
	void ZoomStep(int direction);  //!< +1:ズームイン/-1:ズームアウト。src/Global.cpp の
	                                //!< ZoomRatioList既定値 (10/25/50/75/100/150/200/300/400%)
	                                //!< を段階として使う (src/MainFrm.cpp の
	                                //!< ZoomInIActionExecute/ZoomOutIActionExecute と同じ探索)

	double ComputeFitRatio() const;  //!< フィット時の倍率 (等倍を超えて自動拡大はしない)
	double EffectiveRatio() const;   //!< 現在実際に表示している倍率 (fitted_ なら ComputeFitRatio())
	void RebuildScaledBitmap();      //!< 表示用のスケール済み wxBitmap を作り直す (キャッシュ付き)

	int HeaderHeight() const { return GetCharHeight() + 6; }
	UnicodeString HeaderText() const;

	UnicodeString path_;
	bool has_image_ = false;  //!< 読み込みに成功したか (失敗時はエラー表示のみ)
	UnicodeString error_;

	unsigned int img_w_ = 0, img_h_ = 0;
	std::vector<unsigned char> rgb_;  //!< フルサイズの RGB24 (image_load::LoadForView の結果)

	bool fitted_ = true;      //!< フィット表示 (VCL 版 imgv_thread.cpp コンストラクタの既定値と同じ)
	int zoom_percent_ = 100;  //!< 手動ズーム時の倍率(%)

	wxBitmap scaled_bitmap_;                      //!< 表示用にスケール済みのビットマップ (キャッシュ)
	int scaled_for_w_ = -1, scaled_for_h_ = -1;   //!< scaled_bitmap_ を作った時のクライアントサイズ
	double scaled_ratio_ = 0.0;                   //!< scaled_bitmap_ を作った時の倍率

	std::function<void()> on_close_;
	std::function<void(int)> on_navigate_;
};

#endif  // NYANFI_GUI_IMAGE_VIEWER_H
