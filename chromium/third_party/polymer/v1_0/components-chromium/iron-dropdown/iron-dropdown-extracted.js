(function() {
      'use strict';

      Polymer({
        is: 'iron-dropdown',

        behaviors: [
          Polymer.IronControlState,
          Polymer.IronA11yKeysBehavior,
          Polymer.IronOverlayBehavior,
          Polymer.NeonAnimationRunnerBehavior
        ],

        properties: {
          /**
           * The orientation against which to align the dropdown content
           * horizontally relative to the dropdown trigger.
           */
          horizontalAlign: {
            type: String,
            value: 'left',
            reflectToAttribute: true
          },

          /**
           * The orientation against which to align the dropdown content
           * vertically relative to the dropdown trigger.
           */
          verticalAlign: {
            type: String,
            value: 'top',
            reflectToAttribute: true
          },

          /**
           * A pixel value that will be added to the position calculated for the
           * given `horizontalAlign`, in the direction of alignment. You can think
           * of it as increasing or decreasing the distance to the side of the
           * screen given by `horizontalAlign`.
           *
           * If `horizontalAlign` is "left", this offset will increase or decrease
           * the distance to the left side of the screen: a negative offset will
           * move the dropdown to the left; a positive one, to the right.
           *
           * Conversely if `horizontalAlign` is "right", this offset will increase
           * or decrease the distance to the right side of the screen: a negative
           * offset will move the dropdown to the right; a positive one, to the left.
           */
          horizontalOffset: {
            type: Number,
            value: 0,
            notify: true
          },

          /**
           * A pixel value that will be added to the position calculated for the
           * given `verticalAlign`, in the direction of alignment. You can think
           * of it as increasing or decreasing the distance to the side of the
           * screen given by `verticalAlign`.
           *
           * If `verticalAlign` is "top", this offset will increase or decrease
           * the distance to the top side of the screen: a negative offset will
           * move the dropdown upwards; a positive one, downwards.
           *
           * Conversely if `verticalAlign` is "bottom", this offset will increase
           * or decrease the distance to the bottom side of the screen: a negative
           * offset will move the dropdown downwards; a positive one, upwards.
           */
          verticalOffset: {
            type: Number,
            value: 0,
            notify: true
          },

          /**
           * The element that should be used to position the dropdown when
           * it is opened.
           */
          positionTarget: {
            type: Object,
            observer: '_positionTargetChanged'
          },

          /**
           * An animation config. If provided, this will be used to animate the
           * opening of the dropdown.
           */
          openAnimationConfig: {
            type: Object
          },

          /**
           * An animation config. If provided, this will be used to animate the
           * closing of the dropdown.
           */
          closeAnimationConfig: {
            type: Object
          },

          /**
           * If provided, this will be the element that will be focused when
           * the dropdown opens.
           */
          focusTarget: {
            type: Object
          },

          /**
           * Set to true to disable animations when opening and closing the
           * dropdown.
           */
          noAnimations: {
            type: Boolean,
            value: false
          },

          /**
           * By default, the dropdown will constrain scrolling on the page
           * to itself when opened.
           * Set to true in order to prevent scroll from being constrained
           * to the dropdown when it opens.
           */
          allowOutsideScroll: {
            type: Boolean,
            value: false
          },

          /**
           * We memoize the positionTarget bounding rectangle so that we can
           * limit the number of times it is queried per resize / relayout.
           * @type {?Object}
           */
          _positionRectMemo: {
            type: Object
          }
        },

        listeners: {
          'neon-animation-finish': '_onNeonAnimationFinish'
        },

        observers: [
          '_updateOverlayPosition(verticalAlign, horizontalAlign, verticalOffset, horizontalOffset)'
        ],

        attached: function() {
          if (this.positionTarget === undefined) {
            this.positionTarget = this._defaultPositionTarget;
          }
        },

        /**
         * The element that is contained by the dropdown, if any.
         */
        get containedElement() {
          return Polymer.dom(this.$.content).getDistributedNodes()[0];
        },

        /**
         * The element that should be focused when the dropdown opens.
         */
        get _focusTarget() {
          return this.focusTarget || this.containedElement;
        },

        /**
         * Whether the text direction is RTL
         */
        _isRTL: function() {
          return window.getComputedStyle(this).direction == 'rtl';
        },

        /**
         * The element that should be used to position the dropdown when
         * it opens, if no position target is configured.
         */
        get _defaultPositionTarget() {
          var parent = Polymer.dom(this).parentNode;

          if (parent.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
            parent = parent.host;
          }

          return parent;
        },

        /**
         * The bounding rect of the position target.
         */
        get _positionRect() {
          if (!this._positionRectMemo && this.positionTarget) {
            this._positionRectMemo = this.positionTarget.getBoundingClientRect();
          }

          return this._positionRectMemo;
        },

        /**
         * The horizontal offset value used to position the dropdown.
         */
        get _horizontalAlignTargetValue() {
          var target;

          // In RTL, the direction flips, so what is "right" in LTR becomes "left".
          var isRTL = this._isRTL();
          if ((!isRTL && this.horizontalAlign === 'right') ||
              (isRTL && this.horizontalAlign === 'left')) {
            target = document.documentElement.clientWidth - this._positionRect.right;
          } else {
            target = this._positionRect.left;
          }

          target += this.horizontalOffset;

          return Math.max(target, 0);
        },

        /**
         * The vertical offset value used to position the dropdown.
         */
        get _verticalAlignTargetValue() {
          var target;

          if (this.verticalAlign === 'bottom') {
            target = document.documentElement.clientHeight - this._positionRect.bottom;
          } else {
            target = this._positionRect.top;
          }

          target += this.verticalOffset;

          return Math.max(target, 0);
        },

        /**
         * The horizontal align value, accounting for the RTL/LTR text direction.
         */
        get _localeHorizontalAlign() {
          // In RTL, "left" becomes "right".
          if (this._isRTL()) {
            return this.horizontalAlign === 'right' ? 'left' : 'right';
          } else {
            return this.horizontalAlign;
          }
        },

        /**
         * Called when the value of `opened` changes.
         *
         * @param {boolean} opened True if the dropdown is opened.
         */
        _openedChanged: function(opened) {
          if (opened && this.disabled) {
            this.cancel();
          } else {
            this.cancelAnimation();
            this._prepareDropdown();
            Polymer.IronOverlayBehaviorImpl._openedChanged.apply(this, arguments);
          }

          if (this.opened) {
            this._focusContent();
          }
        },

        /**
         * Overridden from `IronOverlayBehavior`.
         */
        _renderOpened: function() {
          if (!this.allowOutsideScroll) {
            Polymer.IronDropdownScrollManager.pushScrollLock(this);
          }

          if (!this.noAnimations && this.animationConfig && this.animationConfig.open) {
            this.$.contentWrapper.classList.add('animating');
            this.playAnimation('open');
          } else {
            Polymer.IronOverlayBehaviorImpl._renderOpened.apply(this, arguments);
          }
        },

        /**
         * Overridden from `IronOverlayBehavior`.
         */
        _renderClosed: function() {
          Polymer.IronDropdownScrollManager.removeScrollLock(this);
          if (!this.noAnimations && this.animationConfig && this.animationConfig.close) {
            this.$.contentWrapper.classList.add('animating');
            this.playAnimation('close');
          } else {
            Polymer.IronOverlayBehaviorImpl._renderClosed.apply(this, arguments);
          }
        },

        /**
         * Called when animation finishes on the dropdown (when opening or
         * closing). Responsible for "completing" the process of opening or
         * closing the dropdown by positioning it or setting its display to
         * none.
         */
        _onNeonAnimationFinish: function() {
          this.$.contentWrapper.classList.remove('animating');
          if (this.opened) {
            Polymer.IronOverlayBehaviorImpl._renderOpened.apply(this);
          } else {
            Polymer.IronOverlayBehaviorImpl._renderClosed.apply(this);
          }
        },

        /**
         * Called when an `iron-resize` event fires.
         */
        _onIronResize: function() {
          var containedElement = this.containedElement;
          var scrollTop;
          var scrollLeft;

          if (containedElement) {
            scrollTop = containedElement.scrollTop;
            scrollLeft = containedElement.scrollLeft;
          }

          if (this.opened) {
            this._updateOverlayPosition();
          }

          Polymer.IronOverlayBehaviorImpl._onIronResize.apply(this, arguments);

          if (containedElement) {
            containedElement.scrollTop = scrollTop;
            containedElement.scrollLeft = scrollLeft;
          }
        },

        /**
         * Called when the `positionTarget` property changes.
         */
        _positionTargetChanged: function() {
          this._updateOverlayPosition();
        },

        /**
         * Constructs the final animation config from different properties used
         * to configure specific parts of the opening and closing animations.
         */
        _updateAnimationConfig: function() {
          var animationConfig = {};
          var animations = [];

          if (this.openAnimationConfig) {
            // NOTE(cdata): When making `display:none` elements visible in Safari,
            // the element will paint once in a fully visible state, causing the
            // dropdown to flash before it fades in. We prepend an
            // `opaque-animation` to fix this problem:
            animationConfig.open = [{
              name: 'opaque-animation',
            }].concat(this.openAnimationConfig);
            animations = animations.concat(animationConfig.open);
          }

          if (this.closeAnimationConfig) {
            animationConfig.close = this.closeAnimationConfig;
            animations = animations.concat(animationConfig.close);
          }

          animations.forEach(function(animation) {
            animation.node = this.containedElement;
          }, this);

          this.animationConfig = animationConfig;
        },

        /**
         * Prepares the dropdown for opening by updating measured layout
         * values.
         */
        _prepareDropdown: function() {
          this.sizingTarget = this.containedElement || this.sizingTarget;
          this._updateAnimationConfig();
          this._updateOverlayPosition();
        },

        /**
         * Updates the overlay position based on configured horizontal
         * and vertical alignment, and re-memoizes these values for the sake
         * of behavior in `IronFitBehavior`.
         */
        _updateOverlayPosition: function() {
          this._positionRectMemo = null;

          if (!this.positionTarget) {
            return;
          }

          this.style[this._localeHorizontalAlign] =
            this._horizontalAlignTargetValue + 'px';

          this.style[this.verticalAlign] =
            this._verticalAlignTargetValue + 'px';

          // NOTE(cdata): We re-memoize inline styles here, otherwise
          // calling `refit` from `IronFitBehavior` will reset inline styles
          // to whatever they were when the dropdown first opened.
          if (this._fitInfo) {
            this._fitInfo.inlineStyle[this.horizontalAlign] =
              this.style[this.horizontalAlign];

            this._fitInfo.inlineStyle[this.verticalAlign] =
              this.style[this.verticalAlign];
          }
        },

        /**
         * Focuses the configured focus target.
         */
        _focusContent: function() {
          // NOTE(cdata): This is async so that it can attempt the focus after
          // `display: none` is removed from the element.
          this.async(function() {
            if (this._focusTarget) {
              this._focusTarget.focus();
            }
          });
        }
      });
    })();