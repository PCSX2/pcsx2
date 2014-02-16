package com.jeremyfeinstein.slidingmenu.lib;

import java.lang.reflect.Method;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Handler;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

import com.jeremyfeinstein.slidingmenu.lib.CustomViewAbove.OnPageChangeListener;
import com.reicast.loungekatt.R;

public class SlidingMenu extends RelativeLayout {

	private static final String TAG = "SlidingMenu";

	public static final int SLIDING_WINDOW = 0;
	public static final int SLIDING_CONTENT = 1;
	private boolean mActionbarOverlay = false;

	/** Constant value for use with setTouchModeAbove(). Allows the SlidingMenu to be opened with a swipe
	 * gesture on the screen's margin
	 */
	public static final int TOUCHMODE_MARGIN = 0;

	/** Constant value for use with setTouchModeAbove(). Allows the SlidingMenu to be opened with a swipe
	 * gesture anywhere on the screen
	 */
	public static final int TOUCHMODE_FULLSCREEN = 1;

	/** Constant value for use with setTouchModeAbove(). Denies the SlidingMenu to be opened with a swipe
	 * gesture
	 */
	public static final int TOUCHMODE_NONE = 2;

	/** Constant value for use with setMode(). Puts the menu to the left of the content.
	 */
	public static final int LEFT = 0;

	/** Constant value for use with setMode(). Puts the menu to the right of the content.
	 */
	public static final int RIGHT = 1;

	/** Constant value for use with setMode(). Puts menus to the left and right of the content.
	 */
	public static final int LEFT_RIGHT = 2;

	private CustomViewAbove mViewAbove;

	private CustomViewBehind mViewBehind;

	private OnOpenListener mOpenListener;

	private OnCloseListener mCloseListener;

	/**
	 * The listener interface for receiving onOpen events.
	 * The class that is interested in processing a onOpen
	 * event implements this interface, and the object created
	 * with that class is registered with a component using the
	 * component's <code>addOnOpenListener<code> method. When
	 * the onOpen event occurs, that object's appropriate
	 * method is invoked
	 */
	public interface OnOpenListener {

		/**
		 * On open.
		 */
		public void onOpen();
	}

	/**
	 * The listener interface for receiving onOpened events.
	 * The class that is interested in processing a onOpened
	 * event implements this interface, and the object created
	 * with that class is registered with a component using the
	 * component's <code>addOnOpenedListener<code> method. When
	 * the onOpened event occurs, that object's appropriate
	 * method is invoked.
	 *
	 * @see OnOpenedEvent
	 */
	public interface OnOpenedListener {

		/**
		 * On opened.
		 */
		public void onOpened();
	}

	/**
	 * The listener interface for receiving onClose events.
	 * The class that is interested in processing a onClose
	 * event implements this interface, and the object created
	 * with that class is registered with a component using the
	 * component's <code>addOnCloseListener<code> method. When
	 * the onClose event occurs, that object's appropriate
	 * method is invoked.
	 *
	 * @see OnCloseEvent
	 */
	public interface OnCloseListener {

		/**
		 * On close.
		 */
		public void onClose();
	}

	/**
	 * The listener interface for receiving onClosed events.
	 * The class that is interested in processing a onClosed
	 * event implements this interface, and the object created
	 * with that class is registered with a component using the
	 * component's <code>addOnClosedListener<code> method. When
	 * the onClosed event occurs, that object's appropriate
	 * method is invoked.
	 *
	 * @see OnClosedEvent
	 */
	public interface OnClosedListener {

		/**
		 * On closed.
		 */
		public void onClosed();
	}

	/**
	 * The Interface CanvasTransformer.
	 */
	public interface CanvasTransformer {

		/**
		 * Transform canvas.
		 *
		 * @param canvas the canvas
		 * @param percentOpen the percent open
		 */
		public void transformCanvas(Canvas canvas, float percentOpen);
	}

	/**
	 * Instantiates a new SlidingMenu.
	 *
	 * @param context the associated Context
	 */
	public SlidingMenu(Context context) {
		this(context, null);
	}

	/**
	 * Instantiates a new SlidingMenu and attach to Activity.
	 *
	 * @param activity the activity to attach slidingmenu
	 * @param slideStyle the slidingmenu style
	 */
	public SlidingMenu(Activity activity, int slideStyle) {
		this(activity, null);
		this.attachToActivity(activity, slideStyle);
	}

	/**
	 * Instantiates a new SlidingMenu.
	 *
	 * @param context the associated Context
	 * @param attrs the attrs
	 */
	public SlidingMenu(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	/**
	 * Instantiates a new SlidingMenu.
	 *
	 * @param context the associated Context
	 * @param attrs the attrs
	 * @param defStyle the def style
	 */
	public SlidingMenu(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		
		LayoutParams behindParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
		mViewBehind = new CustomViewBehind(context);
		addView(mViewBehind, behindParams);
		LayoutParams aboveParams = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
		mViewAbove = new CustomViewAbove(context);
		addView(mViewAbove, aboveParams);
		// register the CustomViewBehind with the CustomViewAbove
		mViewAbove.setCustomViewBehind(mViewBehind);
		mViewBehind.setCustomViewAbove(mViewAbove);
		mViewAbove.setOnPageChangeListener(new OnPageChangeListener() {
			public static final int POSITION_OPEN = 0;
			public static final int POSITION_CLOSE = 1;

			public void onPageScrolled(int position, float positionOffset,
					int positionOffsetPixels) { }

			public void onPageSelected(int position) {
				if (position == POSITION_OPEN && mOpenListener != null) {
					mOpenListener.onOpen();
				} else if (position == POSITION_CLOSE && mCloseListener != null) {
					mCloseListener.onClose();
				}
			}
		});

		// now style everything!
		TypedArray ta = context.obtainStyledAttributes(attrs, R.styleable.SlidingMenu);
		// set the above and behind views if defined in xml
		int mode = ta.getInt(R.styleable.SlidingMenu_mode, LEFT);
		setMode(mode);
		int viewAbove = ta.getResourceId(R.styleable.SlidingMenu_viewAbove, -1);
		if (viewAbove != -1) {
			setContent(viewAbove);
		} else {
			setContent(new FrameLayout(context));
		}
		int viewBehind = ta.getResourceId(R.styleable.SlidingMenu_viewBehind, -1);
		if (viewBehind != -1) {
			setMenu(viewBehind); 
		} else {
			setMenu(new FrameLayout(context));
		}
		int touchModeAbove = ta.getInt(R.styleable.SlidingMenu_touchModeAbove, TOUCHMODE_MARGIN);
		setTouchModeAbove(touchModeAbove);
		int touchModeBehind = ta.getInt(R.styleable.SlidingMenu_touchModeBehind, TOUCHMODE_MARGIN);
		setTouchModeBehind(touchModeBehind);

		int offsetBehind = (int) ta.getDimension(R.styleable.SlidingMenu_behindOffset, -1);
		int widthBehind = (int) ta.getDimension(R.styleable.SlidingMenu_behindWidth, -1);
		if (offsetBehind != -1 && widthBehind != -1)
			throw new IllegalStateException("Cannot set both behindOffset and behindWidth for a SlidingMenu");
		else if (offsetBehind != -1)
			setBehindOffset(offsetBehind);
		else if (widthBehind != -1)
			setBehindWidth(widthBehind);
		else
			setBehindOffset(0);
		float scrollOffsetBehind = ta.getFloat(R.styleable.SlidingMenu_behindScrollScale, 0.33f);
		setBehindScrollScale(scrollOffsetBehind);
		int shadowRes = ta.getResourceId(R.styleable.SlidingMenu_shadowDrawable, -1);
		if (shadowRes != -1) {
			setShadowDrawable(shadowRes);
		}
		int shadowWidth = (int) ta.getDimension(R.styleable.SlidingMenu_shadowWidth, 0);
		setShadowWidth(shadowWidth);
		boolean fadeEnabled = ta.getBoolean(R.styleable.SlidingMenu_fadeEnabled, true);
		setFadeEnabled(fadeEnabled);
		float fadeDeg = ta.getFloat(R.styleable.SlidingMenu_fadeDegree, 0.33f);
		setFadeDegree(fadeDeg);
		boolean selectorEnabled = ta.getBoolean(R.styleable.SlidingMenu_selectorEnabled, false);
		setSelectorEnabled(selectorEnabled);
		int selectorRes = ta.getResourceId(R.styleable.SlidingMenu_selectorDrawable, -1);
		if (selectorRes != -1)
			setSelectorDrawable(selectorRes);
		ta.recycle();
	}

	/**
	 * Attaches the SlidingMenu to an entire Activity
	 * 
	 * @param activity the Activity
	 * @param slideStyle either SLIDING_CONTENT or SLIDING_WINDOW
	 */
	public void attachToActivity(Activity activity, int slideStyle) {
		attachToActivity(activity, slideStyle, false);
	}

	/**
	 * Attaches the SlidingMenu to an entire Activity
	 * 
	 * @param activity the Activity
	 * @param slideStyle either SLIDING_CONTENT or SLIDING_WINDOW
	 * @param actionbarOverlay whether or not the ActionBar is overlaid
	 */
	public void attachToActivity(Activity activity, int slideStyle, boolean actionbarOverlay) {
		if (slideStyle != SLIDING_WINDOW && slideStyle != SLIDING_CONTENT)
			throw new IllegalArgumentException("slideStyle must be either SLIDING_WINDOW or SLIDING_CONTENT");

		if (getParent() != null)
			throw new IllegalStateException("This SlidingMenu appears to already be attached");

		// get the window background
		TypedArray a = activity.getTheme().obtainStyledAttributes(new int[] {android.R.attr.windowBackground});
		int background = a.getResourceId(0, 0);
		a.recycle();

		switch (slideStyle) {
		case SLIDING_WINDOW:
			mActionbarOverlay = false;
			ViewGroup decor = (ViewGroup) activity.getWindow().getDecorView();
			ViewGroup decorChild = (ViewGroup) decor.getChildAt(0);
			// save ActionBar themes that have transparent assets
			decorChild.setBackgroundResource(background);
			decor.removeView(decorChild);
			decor.addView(this);
			setContent(decorChild);
			break;
		case SLIDING_CONTENT:
			mActionbarOverlay = actionbarOverlay;
			// take the above view out of
			ViewGroup contentParent = (ViewGroup)activity.findViewById(android.R.id.content);
			View content = contentParent.getChildAt(0);
			contentParent.removeView(content);
			contentParent.addView(this);
			setContent(content);
			// save people from having transparent backgrounds
			if (content.getBackground() == null)
				content.setBackgroundResource(background);
			break;
		}
	}

	/**
	 * Set the above view content from a layout resource. The resource will be inflated, adding all top-level views
	 * to the above view.
	 *
	 * @param res the new content
	 */
	public void setContent(int res) {
		setContent(LayoutInflater.from(getContext()).inflate(res, null));
	}

	/**
	 * Set the above view content to the given View.
	 *
	 * @param view The desired content to display.
	 */
	public void setContent(View view) {
		mViewAbove.setContent(view);
		showContent();
	}

	/**
	 * Retrieves the current content.
	 * @return the current content
	 */
	public View getContent() {
		return mViewAbove.getContent();
	}

	/**
	 * Set the behind view (menu) content from a layout resource. The resource will be inflated, adding all top-level views
	 * to the behind view.
	 *
	 * @param res the new content
	 */
	public void setMenu(int res) {
		setMenu(LayoutInflater.from(getContext()).inflate(res, null));
	}

	/**
	 * Set the behind view (menu) content to the given View.
	 *
	 * @param view The desired content to display.
	 */
	public void setMenu(View v) {
		mViewBehind.setContent(v);
	}

	/**
	 * Retrieves the main menu.
	 * @return the main menu
	 */
	public View getMenu() {
		return mViewBehind.getContent();
	}

	/**
	 * Set the secondary behind view (right menu) content from a layout resource. The resource will be inflated, adding all top-level views
	 * to the behind view.
	 *
	 * @param res the new content
	 */
	public void setSecondaryMenu(int res) {
		setSecondaryMenu(LayoutInflater.from(getContext()).inflate(res, null));
	}

	/**
	 * Set the secondary behind view (right menu) content to the given View.
	 *
	 * @param view The desired content to display.
	 */
	public void setSecondaryMenu(View v) {
		mViewBehind.setSecondaryContent(v);
		//		mViewBehind.invalidate();
	}

	/**
	 * Retrieves the current secondary menu (right).
	 * @return the current menu
	 */
	public View getSecondaryMenu() {
		return mViewBehind.getSecondaryContent();
	}


	/**
	 * Sets the sliding enabled.
	 *
	 * @param b true to enable sliding, false to disable it.
	 */
	public void setSlidingEnabled(boolean b) {
		mViewAbove.setSlidingEnabled(b);
	}

	/**
	 * Checks if is sliding enabled.
	 *
	 * @return true, if is sliding enabled
	 */
	public boolean isSlidingEnabled() {
		return mViewAbove.isSlidingEnabled();
	}

	/**
	 * Sets which side the SlidingMenu should appear on.
	 * @param mode must be either SlidingMenu.LEFT or SlidingMenu.RIGHT
	 */
	public void setMode(int mode) {
		if (mode != LEFT && mode != RIGHT && mode != LEFT_RIGHT) {
			throw new IllegalStateException("SlidingMenu mode must be LEFT, RIGHT, or LEFT_RIGHT");
		}
		mViewBehind.setMode(mode);
	}

	/**
	 * Returns the current side that the SlidingMenu is on.
	 * @return the current mode, either SlidingMenu.LEFT or SlidingMenu.RIGHT
	 */
	public int getMode() {
		return mViewBehind.getMode();
	}

	/**
	 * Sets whether or not the SlidingMenu is in static mode (i.e. nothing is moving and everything is showing)
	 *
	 * @param b true to set static mode, false to disable static mode.
	 */
	public void setStatic(boolean b) {
		if (b) {
			setSlidingEnabled(false);
			mViewAbove.setCustomViewBehind(null);
			mViewAbove.setCurrentItem(1);
			//			mViewBehind.setCurrentItem(0);	
		} else {
			mViewAbove.setCurrentItem(1);
			//			mViewBehind.setCurrentItem(1);
			mViewAbove.setCustomViewBehind(mViewBehind);
			setSlidingEnabled(true);
		}
	}

	/**
	 * Opens the menu and shows the menu view.
	 */
	public void showMenu() {
		showMenu(true);
	}

	/**
	 * Opens the menu and shows the menu view.
	 *
	 * @param animate true to animate the transition, false to ignore animation
	 */
	public void showMenu(boolean animate) {
		mViewAbove.setCurrentItem(0, animate);
	}

	/**
	 * Opens the menu and shows the secondary menu view. Will default to the regular menu
	 * if there is only one.
	 */
	public void showSecondaryMenu() {
		showSecondaryMenu(true);
	}

	/**
	 * Opens the menu and shows the secondary (right) menu view. Will default to the regular menu
	 * if there is only one.
	 *
	 * @param animate true to animate the transition, false to ignore animation
	 */
	public void showSecondaryMenu(boolean animate) {
		mViewAbove.setCurrentItem(2, animate);
	}

	/**
	 * Closes the menu and shows the above view.
	 */
	public void showContent() {
		showContent(true);
	}

	/**
	 * Closes the menu and shows the above view.
	 *
	 * @param animate true to animate the transition, false to ignore animation
	 */
	public void showContent(boolean animate) {
		mViewAbove.setCurrentItem(1, animate);
	}

	/**
	 * Toggle the SlidingMenu. If it is open, it will be closed, and vice versa.
	 */
	public void toggle() {
		toggle(true);
	}

	/**
	 * Toggle the SlidingMenu. If it is open, it will be closed, and vice versa.
	 *
	 * @param animate true to animate the transition, false to ignore animation
	 */
	public void toggle(boolean animate) {
		if (isMenuShowing()) {
			showContent(animate);
		} else {
			showMenu(animate);
		}
	}

	/**
	 * Checks if is the behind view showing.
	 *
	 * @return Whether or not the behind view is showing
	 */
	public boolean isMenuShowing() {
		return mViewAbove.getCurrentItem() == 0 || mViewAbove.getCurrentItem() == 2;
	}
	
	/**
	 * Checks if is the behind view showing.
	 *
	 * @return Whether or not the behind view is showing
	 */
	public boolean isSecondaryMenuShowing() {
		return mViewAbove.getCurrentItem() == 2;
	}

	/**
	 * Gets the behind offset.
	 *
	 * @return The margin on the right of the screen that the behind view scrolls to
	 */
	public int getBehindOffset() {
		return ((RelativeLayout.LayoutParams)mViewBehind.getLayoutParams()).rightMargin;
	}

	/**
	 * Sets the behind offset.
	 *
	 * @param i The margin, in pixels, on the right of the screen that the behind view scrolls to.
	 */
	public void setBehindOffset(int i) {
		//		RelativeLayout.LayoutParams params = ((RelativeLayout.LayoutParams)mViewBehind.getLayoutParams());
		//		int bottom = params.bottomMargin;
		//		int top = params.topMargin;
		//		int left = params.leftMargin;
		//		params.setMargins(left, top, i, bottom);
		mViewBehind.setWidthOffset(i);
	}

	/**
	 * Sets the behind offset.
	 *
	 * @param resID The dimension resource id to be set as the behind offset.
	 * The menu, when open, will leave this width margin on the right of the screen.
	 */
	public void setBehindOffsetRes(int resID) {
		int i = (int) getContext().getResources().getDimension(resID);
		setBehindOffset(i);
	}

	/**
	 * Sets the above offset.
	 *
	 * @param i the new above offset, in pixels
	 */
	public void setAboveOffset(int i) {
		mViewAbove.setAboveOffset(i);
	}

	/**
	 * Sets the above offset.
	 *
	 * @param resID The dimension resource id to be set as the above offset.
	 */
	public void setAboveOffsetRes(int resID) {
		int i = (int) getContext().getResources().getDimension(resID);
		setAboveOffset(i);
	}

	/**
	 * Sets the behind width.
	 *
	 * @param i The width the Sliding Menu will open to, in pixels
	 */
	@SuppressWarnings("deprecation")
	public void setBehindWidth(int i) {
		int width;
		Display display = ((WindowManager) getContext().getSystemService(Context.WINDOW_SERVICE))
				.getDefaultDisplay();
		try {
			Class<?> cls = Display.class;
			Class<?>[] parameterTypes = {Point.class};
			Point parameter = new Point();
			Method method = cls.getMethod("getSize", parameterTypes);
			method.invoke(display, parameter);
			width = parameter.x;
		} catch (Exception e) {
			width = display.getWidth();
		}
		setBehindOffset(width-i);
	}

	/**
	 * Sets the behind width.
	 *
	 * @param res The dimension resource id to be set as the behind width offset.
	 * The menu, when open, will open this wide.
	 */
	public void setBehindWidthRes(int res) {
		int i = (int) getContext().getResources().getDimension(res);
		setBehindWidth(i);
	}

	/**
	 * Gets the behind scroll scale.
	 *
	 * @return The scale of the parallax scroll
	 */
	public float getBehindScrollScale() {
		return mViewBehind.getScrollScale();
	}
	
	/**
	 * Gets the touch mode margin threshold
	 * @return the touch mode margin threshold
	 */
	public int getTouchmodeMarginThreshold() {
		return mViewBehind.getMarginThreshold();
	}
	
	/**
	 * Set the touch mode margin threshold
	 * @param touchmodeMarginThreshold
	 */
	public void setTouchmodeMarginThreshold(int touchmodeMarginThreshold) {
		mViewBehind.setMarginThreshold(touchmodeMarginThreshold);
	}

	/**
	 * Sets the behind scroll scale.
	 *
	 * @param f The scale of the parallax scroll (i.e. 1.0f scrolls 1 pixel for every
	 * 1 pixel that the above view scrolls and 0.0f scrolls 0 pixels)
	 */
	public void setBehindScrollScale(float f) {
		if (f < 0 && f > 1)
			throw new IllegalStateException("ScrollScale must be between 0 and 1");
		mViewBehind.setScrollScale(f);
	}

	/**
	 * Sets the behind canvas transformer.
	 *
	 * @param t the new behind canvas transformer
	 */
	public void setBehindCanvasTransformer(CanvasTransformer t) {
		mViewBehind.setCanvasTransformer(t);
	}

	/**
	 * Gets the touch mode above.
	 *
	 * @return the touch mode above
	 */
	public int getTouchModeAbove() {
		return mViewAbove.getTouchMode();
	}

	/**
	 * Controls whether the SlidingMenu can be opened with a swipe gesture.
	 * Options are {@link #TOUCHMODE_MARGIN TOUCHMODE_MARGIN}, {@link #TOUCHMODE_FULLSCREEN TOUCHMODE_FULLSCREEN},
	 * or {@link #TOUCHMODE_NONE TOUCHMODE_NONE}
	 *
	 * @param i the new touch mode
	 */
	public void setTouchModeAbove(int i) {
		if (i != TOUCHMODE_FULLSCREEN && i != TOUCHMODE_MARGIN
				&& i != TOUCHMODE_NONE) {
			throw new IllegalStateException("TouchMode must be set to either" +
					"TOUCHMODE_FULLSCREEN or TOUCHMODE_MARGIN or TOUCHMODE_NONE.");
		}
		mViewAbove.setTouchMode(i);
	}

	/**
	 * Controls whether the SlidingMenu can be opened with a swipe gesture.
	 * Options are {@link #TOUCHMODE_MARGIN TOUCHMODE_MARGIN}, {@link #TOUCHMODE_FULLSCREEN TOUCHMODE_FULLSCREEN},
	 * or {@link #TOUCHMODE_NONE TOUCHMODE_NONE}
	 *
	 * @param i the new touch mode
	 */
	public void setTouchModeBehind(int i) {
		if (i != TOUCHMODE_FULLSCREEN && i != TOUCHMODE_MARGIN
				&& i != TOUCHMODE_NONE) {
			throw new IllegalStateException("TouchMode must be set to either" +
					"TOUCHMODE_FULLSCREEN or TOUCHMODE_MARGIN or TOUCHMODE_NONE.");
		}
		mViewBehind.setTouchMode(i);
	}

	/**
	 * Sets the shadow drawable.
	 *
	 * @param resId the resource ID of the new shadow drawable
	 */
	public void setShadowDrawable(int resId) {
		setShadowDrawable(getContext().getResources().getDrawable(resId));
	}

	/**
	 * Sets the shadow drawable.
	 *
	 * @param d the new shadow drawable
	 */
	public void setShadowDrawable(Drawable d) {
		mViewBehind.setShadowDrawable(d);
	}

	/**
	 * Sets the secondary (right) shadow drawable.
	 *
	 * @param resId the resource ID of the new shadow drawable
	 */
	public void setSecondaryShadowDrawable(int resId) {
		setSecondaryShadowDrawable(getContext().getResources().getDrawable(resId));
	}

	/**
	 * Sets the secondary (right) shadow drawable.
	 *
	 * @param d the new shadow drawable
	 */
	public void setSecondaryShadowDrawable(Drawable d) {
		mViewBehind.setSecondaryShadowDrawable(d);
	}

	/**
	 * Sets the shadow width.
	 *
	 * @param resId The dimension resource id to be set as the shadow width.
	 */
	public void setShadowWidthRes(int resId) {
		setShadowWidth((int)getResources().getDimension(resId));
	}

	/**
	 * Sets the shadow width.
	 *
	 * @param pixels the new shadow width, in pixels
	 */
	public void setShadowWidth(int pixels) {
		mViewBehind.setShadowWidth(pixels);
	}

	/**
	 * Enables or disables the SlidingMenu's fade in and out
	 *
	 * @param b true to enable fade, false to disable it
	 */
	public void setFadeEnabled(boolean b) {
		mViewBehind.setFadeEnabled(b);
	}

	/**
	 * Sets how much the SlidingMenu fades in and out. Fade must be enabled, see
	 * {@link #setFadeEnabled(boolean) setFadeEnabled(boolean)}
	 *
	 * @param f the new fade degree, between 0.0f and 1.0f
	 */
	public void setFadeDegree(float f) {
		mViewBehind.setFadeDegree(f);
	}

	/**
	 * Enables or disables whether the selector is drawn
	 *
	 * @param b true to draw the selector, false to not draw the selector
	 */
	public void setSelectorEnabled(boolean b) {
		mViewBehind.setSelectorEnabled(true);
	}

	/**
	 * Sets the selected view. The selector will be drawn here
	 *
	 * @param v the new selected view
	 */
	public void setSelectedView(View v) {
		mViewBehind.setSelectedView(v);
	}

	/**
	 * Sets the selector drawable.
	 *
	 * @param res a resource ID for the selector drawable
	 */
	public void setSelectorDrawable(int res) {
		mViewBehind.setSelectorBitmap(BitmapFactory.decodeResource(getResources(), res));
	}

	/**
	 * Sets the selector drawable.
	 *
	 * @param b the new selector bitmap
	 */
	public void setSelectorBitmap(Bitmap b) {
		mViewBehind.setSelectorBitmap(b);
	}

	/**
	 * Add a View ignored by the Touch Down event when mode is Fullscreen
	 *
	 * @param v a view to be ignored
	 */
	public void addIgnoredView(View v) {
		mViewAbove.addIgnoredView(v);
	}

	/**
	 * Remove a View ignored by the Touch Down event when mode is Fullscreen
	 *
	 * @param v a view not wanted to be ignored anymore
	 */
	public void removeIgnoredView(View v) {
		mViewAbove.removeIgnoredView(v);
	}

	/**
	 * Clear the list of Views ignored by the Touch Down event when mode is Fullscreen
	 */
	public void clearIgnoredViews() {
		mViewAbove.clearIgnoredViews();
	}

	/**
	 * Sets the OnOpenListener. {@link OnOpenListener#onOpen() OnOpenListener.onOpen()} will be called when the SlidingMenu is opened
	 *
	 * @param listener the new OnOpenListener
	 */
	public void setOnOpenListener(OnOpenListener listener) {
		//mViewAbove.setOnOpenListener(listener);
		mOpenListener = listener;
	}

	/**
	 * Sets the OnCloseListener. {@link OnCloseListener#onClose() OnCloseListener.onClose()} will be called when the SlidingMenu is closed
	 *
	 * @param listener the new setOnCloseListener
	 */
	public void setOnCloseListener(OnCloseListener listener) {
		//mViewAbove.setOnCloseListener(listener);
		mCloseListener = listener;
	}

	/**
	 * Sets the OnOpenedListener. {@link OnOpenedListener#onOpened() OnOpenedListener.onOpened()} will be called after the SlidingMenu is opened
	 *
	 * @param listener the new OnOpenedListener
	 */
	public void setOnOpenedListener(OnOpenedListener listener) {
		mViewAbove.setOnOpenedListener(listener);
	}

	/**
	 * Sets the OnClosedListener. {@link OnClosedListener#onClosed() OnClosedListener.onClosed()} will be called after the SlidingMenu is closed
	 *
	 * @param listener the new OnClosedListener
	 */
	public void setOnClosedListener(OnClosedListener listener) {
		mViewAbove.setOnClosedListener(listener);
	}

	public static class SavedState extends BaseSavedState {

		private final int mItem;

		public SavedState(Parcelable superState, int item) {
			super(superState);
			mItem = item;
		}

		private SavedState(Parcel in) {
			super(in);
			mItem = in.readInt();
		}

		public int getItem() {
			return mItem;
		}

		/* (non-Javadoc)
		 * @see android.view.AbsSavedState#writeToParcel(android.os.Parcel, int)
		 */
		public void writeToParcel(Parcel out, int flags) {
			super.writeToParcel(out, flags);
			out.writeInt(mItem);
		}

		public static final Parcelable.Creator<SavedState> CREATOR =
				new Parcelable.Creator<SavedState>() {
			public SavedState createFromParcel(Parcel in) {
				return new SavedState(in);
			}

			public SavedState[] newArray(int size) {
				return new SavedState[size];
			}
		};

	}

	/* (non-Javadoc)
	 * @see android.view.View#onSaveInstanceState()
	 */
	@Override
	protected Parcelable onSaveInstanceState() {
		Parcelable superState = super.onSaveInstanceState();
		SavedState ss = new SavedState(superState, mViewAbove.getCurrentItem());
		return ss;
	}

	/* (non-Javadoc)
	 * @see android.view.View#onRestoreInstanceState(android.os.Parcelable)
	 */
	@Override
	protected void onRestoreInstanceState(Parcelable state) {
		SavedState ss = (SavedState)state;
		super.onRestoreInstanceState(ss.getSuperState());
		mViewAbove.setCurrentItem(ss.getItem());
	}

	/* (non-Javadoc)
	 * @see android.view.ViewGroup#fitSystemWindows(android.graphics.Rect)
	 */
	@SuppressLint("NewApi")
	@Override
	protected boolean fitSystemWindows(Rect insets) {
		int leftPadding = insets.left;
		int rightPadding = insets.right;
		int topPadding = insets.top;
		int bottomPadding = insets.bottom;
		if (!mActionbarOverlay) {
			Log.v(TAG, "setting padding!");
			setPadding(leftPadding, topPadding, rightPadding, bottomPadding);
		}
		return true;
	}
	
	private Handler mHandler = new Handler();

	@TargetApi(Build.VERSION_CODES.HONEYCOMB)
	public void manageLayers(float percentOpen) {
		if (Build.VERSION.SDK_INT < 11) return;

		boolean layer = percentOpen > 0.0f && percentOpen < 1.0f;
		final int layerType = layer ? View.LAYER_TYPE_HARDWARE : View.LAYER_TYPE_NONE;

		if (layerType != getContent().getLayerType()) {
			mHandler.post(new Runnable() {
				public void run() {
					Log.v(TAG, "changing layerType. hardware? " + (layerType == View.LAYER_TYPE_HARDWARE));
					getContent().setLayerType(layerType, null);
					getMenu().setLayerType(layerType, null);
					if (getSecondaryMenu() != null) {
						getSecondaryMenu().setLayerType(layerType, null);
					}
				}
			});
		}
	}

}