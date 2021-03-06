EUI = require('eui');
ajax = require('ajax');
Property = require('class').Property;

const reddit_url = 'http://www.reddit.com/';
saved_reddits = new EUI.DBModel('reddit');

/** @extends EUI.Model */
RSSModel = EUI.Model({
  /**
   * @param {String} url
   * @param {String} path
   * @event
   */
  init: function(url, path) {
    this.url = this.url || url;
    this._array = [];
    this.refresh();
  },
  /**
   * Refresh model after finishing XML file download
   * @event
   */
  refresh: function() {
    ajax.ajax(this.url, {
      onSuccess: function(request) {
        if (!request.responseXML)
          return;

        this._array = request.responseXML.rss.channel.item;
        this.notifyListeners();
      }.bind(this)
    });
  },
  /** @inheritdoc */
  length: function() {
    return this._array.length;
  },
  /** @inheritdoc */
  itemAtIndex: function(index) {
    return this._array[index];
  }
});

/** @extends EUI.ListController */
RSSList = EUI.ListController({
  /** @type {String} */
  style: 'text_label',
  mode: 'compress',
  /** @type {String} */
  group_name: 'RSS',
  /**
   * @param {String} url
   * @param {String} title
   * @event
   */
  init: function(url, title) {
    this.title = title;
    this.model = new RSSModel(url);
  },
  /** @inheritdoc */
  itemAtIndex: function(index) {
    var item = this.model.itemAtIndex(index);

    return {
      text: item.title,
      text_sub: item.description
    };
  },
  /** @inheritdoc */
  selectedItemAtIndex: function(index) {
    var item = this.model.itemAtIndex(index);
    this.pushController(new WebBrowser(item.link));
  },
  /** @type {Object} */
  navigationBarItems: { left: 'sidePanel' }
});

/** @extends EUI.Model */
RedditModel = EUI.Model({
  /**
   * @param {String} endpoint
   * @event
   */
  init: function(endpoint) {
    this.items = [];
    this.url = reddit_url + endpoint;

    this.refresh();
  },
  /**
   * Refresh model after finishing HTML file download
   */
  refresh: function() {
    ajax.get(this.url, null,
      function(request) {
        if (!request.responseJSON)
          return;

        this.items = request.responseJSON.data.children;
        this.notifyListeners();
      }.bind(this)
    );
  },
  /** @inheritdoc */
  length: function() {
    return this.items.length;
  },
  /** @inheritdoc */
  itemAtIndex: function(index) {
    return this.items[index].data;
  }
});

/** @extends EUI.WebController */
WebBrowser = EUI.WebController({
});

/** @extends EUI.ListController */
RedditList = EUI.ListController({
  /** @type {String} */
  style: 'text_label',
  mode: 'compress',
  /** @type {String} */
  group_name: 'Subreddits',
  /**
   * @param {String} endpoint
   * @param {String} title
   * @param {String} icon
   * @event
   */
  init: function(endpoint, title, icon, mode) {
    this.model = new RedditModel(endpoint);
    this.title = title;
    this.icon = icon || 'emblem-generic';
    this.mode = mode || 'subscribed';
    this.endpoint = endpoint;
  },
  /** @inheritdoc */
  itemAtIndex: function(index) {
    var item = this.model.itemAtIndex(index);
    return {
      text: item.title,
      text_sub: 'Posted by ' + item.author + ' in ' + item.subreddit
    };
  },
  /** @inheritdoc */
  selectedItemAtIndex: function(index) {
    var url = this.model.itemAtIndex(index).permalink;
    if (url)
      url = reddit_url + url + '.compact';
    else
      url = item.url;
    this.pushController(new WebBrowser(url));
  },
  /** @type {Object} */
  navigationBarItems: function() {
    if (this.mode === 'subscribed')
      return { left: 'sidePanel', right: 'Refresh' };
    return { right: 'Subscribe' };
  },
  /** @inheritdoc */
  selectedNavigationBarItem: function(item) {
    if (item === 'Refresh') {
      this.model.refresh();
      return;
    }

    if (item === 'Subscribe') {
      this.popController();
      this.parent.popController();

      reddits.pushItem(this.clone(), true);
      saved_reddits.insert({
        endpoint: this.endpoint,
        title: this.title,
        icon: this.icon,
        mode: this.mode
      });
    }
  },
  /** @type{RedditList} */
  clone: function() {
    return new RedditList(this.endpoint, this.title, this.icon);
  },
});

/** @extends EUI.Model */
SearchModel = EUI.Model({
  /** @inheritdoc */
  init: function() {
    this.array = [];
  },
  /** @inheritdoc */
  itemAtIndex: function(index) {
    if (index < this.array.length)
      return this.array[index].data;
  },
  /** @inheritdoc */
  length: function() {
    return this.array.length;
  },
  /** @type {String} */
  filter: new Property ({
    set: function(terms) {
      this.array = [];
      ajax.get(reddit_url + 'reddits/search.json', { "q": terms }, function(request) {
        if (!request.responseJSON)
          return;

        this.array = request.responseJSON.data.children;
        this.notifyListeners();
      }.bind(this));
    }
  })
});

/** @extends EUI.ListController */
RedditSearchController = EUI.ListController({
  /** @type {String} */
  title: 'Search',
  /** @type {SearchModel} */
  model: new SearchModel(),
  /** @inheritdoc */
  itemAtIndex: function(index) {
    var item = this.model.itemAtIndex(index);
    if (!item) return;
    return {
      text: item.display_name
    };
  },
  /** @inheritdoc */
  selectedItemAtIndex: function(index) {
    var subreddit = this.model.itemAtIndex(index);
    this.pushController(new RedditList(subreddit.url + '/top.json',
        subreddit.title, null, 'pre-subscription'));
  },
  /**
   * @param {String} text
   */
  search: function(text) {
    if (this.filterSetTimeout)
      clearTimeout(this.filterSetTimeout);
    this.filterSetTimeout = setTimeout(function() {
      delete this.filterSetTimeout;
      this.model.filter = text;
    }.bind(this), 200);
  }
});

/** @extends EUI.ArrayModel */
if(!(saved_reddits.length)) {
  default_reddits = [
    {endpoint: 'r/top.json', title: 'Home', icon: 'go-home'},
    {endpoint: 'r/programming/top.json', title: 'Proggit', icon: 'applications-development'},
    {endpoint: 'r/iama/top.json', title: 'IAmA', icon: 'dialog-question'},
    {endpoint: 'r/funny/top.json', title: 'Funny', icon: 'face-laugh'},
    {endpoint: 'http://feeds.bbci.co.uk/news/uk/rss.xml',title: 'BBC'}
  ];
  for (var i in default_reddits)
    saved_reddits.insert(default_reddits[i]);
}

reddits = new EUI.ArrayModel([]);
for(var index = 0; index < saved_reddits.length; index++) {
  var item = saved_reddits.itemAtIndex(index);
  reddits.pushItem(new RedditList(item.endpoint, item.title, item.icon));
}

/** @extends EUI.ListController */
Subscriptions = EUI.ListController({
  /** @type {String} */
  group_name: 'Management',
  /** @type {String} */
  title: 'Subscriptions',
  /** @type {EUI.FilterModel} */
  model: new EUI.FilterModel(reddits,
    function(item) {
      return item.group_name === 'Subreddits';
    }
  ),
  /** @type {Object} */
  navigationBarItems: { left: 'sidePanel', right: 'New' },
  /** @inheritdoc */
  selectedNavigationBarItem: function(item) {
    if (item === 'New')
      this.pushController(new RedditSearchController());
  },
  /** @inheritdoc */
  itemAtIndex: function(index) {
    var item = this.model.itemAtIndex(index);
    return {
      text: item.title,
      icon: item.icon
    };
  },
  contextMenuItems: ['Unsubscribe'],
  selectedContextMenuItem: function(menuItem, index) {
    if(menuItem == 'Unsubscribe') {
      var item = this.model.itemAtIndex(index);
      var i = reddits.indexOf(item);
      reddits.deleteItemAtIndex(i);
      saved_reddits.deleteItemAtIndex(index);
    }
  },
});

reddits.pushItem(new Subscriptions());

/** @extends EUI.ListController */
RedditAppSwitch = new EUI.ListController({
  /** @type {EUI.ArrayModel} */
  model: reddits,
  /** @inheritdoc */
  itemAtIndex: function(index) {
    var item = this.model.itemAtIndex(index);
    return {
      text: item.title,
      icon: item.icon,
      group: item.group_name,
      badge: {
        text: item.model.length,
        color: {red: 255, green: 255, blue: 255}
      }
    };
  },
  /** @inheritdoc */
  selectedItemAtIndex: function(index) {
    this.model.selectedIndex = index;
    this.split.leftPanelVisible = false;
  }
});

/** @extends EUI.FrameController */
RedditAppTabs = new EUI.FrameController({
  /** @type {EUI.ArrayModel} */
  model: reddits,
});

/** @extends EUI.SplitController */
Reddit = EUI.SplitController({
  /** @type {EUI.ArrayModel} */
  model: new EUI.ArrayModel([
    new RedditAppSwitch(),
    new RedditAppTabs()
  ])
});

EUI.app(new Reddit());
