/**
 * @file compat/vcl_forward.h
 * @brief VCL の GUI コントロールの前方宣言
 *
 * ロジック層のヘッダには「GUI コントロールをポインタ引数として受けるだけ」の
 * 宣言が混ざっている (usr_str.h の `get_WidthInPanel(..., TPanel *pp)`、
 * usr_color.h の `set_EditColor(TEdit *ep)`、usr_shell.h の
 * `AddNew(TForm *form, TWinControl *ctrl)` など)。宣言を読むだけなら
 * 前方宣言で足りるので、Phase 0 ではここで宣言だけ通す。
 *
 * これらの実体は Phase 2 以降で wxWidgets のコントロールに置き換える。
 * 実装まで必要なファイル (usr_color.cpp の set_EditColor 群など) は Phase 0 の
 * 対象外として docs/port/phase0-report.md に記録する。
 *
 * 注意: TCanvas / TBitmap / TColor は compat/graphics.h が namespace Graphics に
 * 定義しているので、ここでは宣言しない (別の型になってしまう)。
 */
#ifndef NYANFI_COMPAT_VCL_FORWARD_H
#define NYANFI_COMPAT_VCL_FORWARD_H

class TComponent;
class TForm;
class TWinControl;
class TControl;
class TPanel;
class TEdit;
class TLabeledEdit;
class TMaskEdit;
class TMemo;
class TRichEdit;
class TComboBox;
class TCustomListBox;
class TListBox;
class TCheckListBox;
class TStringGrid;
class TDrawGrid;
class TImage;
class TLabel;
class TPaintBox;
class TToolBar;
class TTabControl;
class TTreeView;
class THeaderControl;
class TStatusBar;
class TPageControl;
class TTabSheet;
class TAction;
class TCheckBox;
class TRadioGroup;
class TMenuItem;
class TPopupMenu;
class TScrollBar;
class TTimer;
class TProgressBar;
class TTrackBar;
class TCustomEdit;
class TSplitter;
class TUpDown;
class TApplication;
class TScreen;

#endif  // NYANFI_COMPAT_VCL_FORWARD_H
