/*
 * Namespace
 */
if (!httpd) {
  var httpd = new Object();
}
/*
 * Utils
 */
httpd.size = function(size) {
	if (size < 1)return '0Kb';
	if (size < 1024)return '1Kb';
	if (size < 1024*1024)return new Number(size/1024).toFixed(1) + 'Kb';
	return new Number(size/(1024*1024)).toFixed(1) + 'Mb';
}
httpd.validate = function(input) {
	// none of these are valid in path names and thus indicate
	// someone playing sillybuggers
	if (input.indexOf('<') >= 0 ) {
		throw e;
	}
	if (input.indexOf('>') >= 0 ) {
		throw e;
	}
	return input;
}
httpd.renderUploading = function() {
	jQuery('#upload-form')[0].style.display='none';
	var output = '';
	output += '<img id="upload-throbber" src="/res/img/upload-throbber.gif" />';
	jQuery('#upload-form').after(output);
}

/**
 * @class httpd.Directory
 * @constructor
 * @param domParent jQuery object representing the element where this directory is shown
 * @param filename the path of the directory it represents
 */
httpd.Directory = function(domParent, path) {
	this.dom = domParent;
	this.data = {
		loaded: false,
		path: path,
		contents: [],
		directories: []
	};
}

httpd.Directory.prototype.init = function(basedir) {
	// TODO render throbber
	this.dom.click( function() {
		if(typeof(uploader) != "undefined")uploader.renderIcon(basedir);
		return false;
	} );
	this.expand();
};

httpd.Directory.prototype.renderContents = function() {
	if (this.data.contents.length == 1) {
		this.dom.find('img')[0].src = '/res/mime/folder_grey.png'
		return;
	}
	var output = '';
	output += '<ul>';
	for( file in this.data.contents ) {
		if (file == 0)continue; // first index is the back link
		var file = this.data.contents[file];
		output += '<li class="' + (file.dir ? 'dir' : 'file') + '">';
		/* right mouse click open in a new tab works
		if ( file.dir ) {
			output += '<span><a href="' + encodeURI(this.data.path + file.name) + '">';
			output += '<img src="/res/img/forward.png"/></a></span>';
		}
		*/
		output += '<a class="flink" href="' + encodeURI(this.data.path + file.name) + '"><img src="' + encodeURI(file.icon) + '" />';
		// TODO escape HTML properly (in theory file names have no characters that need escaping provided we are UTF-8)
		output += file.name.replace(' ', '&nbsp;').replace('/', '');
		if ( ! file.dir ) {
			output += '<span class="tooltip">Size: ' +
				httpd.size(file.size);
			if (file.last_mod) {
				output += ' Last modified: ' + file.last_mod;
			}
			output += '</span>';
		}
		output += '</a></li>';
	}
	output += '</ul>';
	this.dom.after(output);
	var anchors = this.dom.parent().find('ul > li.dir > a');
	var parent = this;
	for (i = 0; i < anchors.length; i++) {
		var dom = jQuery(anchors[i]);
		var path = this.data.path + this.data.contents[i + 1].name;
		var subDir = new httpd.Directory(dom, path);
		this.data.directories.push(subDir);
		dom.click( function() {
			var dir = parent.findDirectory(this);
			if ( ! dir.data.loaded) {
				dir.expand();
			} else {
				dir.contract();
			}
			if(typeof(uploader) != "undefined")uploader.renderIcon(dir.data.path);
			return false;
		} );
	}
	this.data.loaded = true;
	this.dom.find('img')[0].src = '/res/mime/folder.png';
};

	/**
	 * Load the directory index
	 */
httpd.Directory.prototype.expand = function() {
	this.dom.find('img')[0].src = '/res/img/throbber.gif';
	var self = this;
	var req = {
		url: this.data.path,
		dataType: 'json',
		data: "json=true",
		success: function(response) {
			//self.data.name = response.path;
			self.data.contents = response.files;
			self.renderContents();
		},
		error: function(response){
			alert('error');
		},
		beforeSend: function(xmlHttpRequest) {
			xmlHttpRequest.setRequestHeader('X-JsonIndex', 'true');
		}
	};
	jQuery.ajax(req);
	jQuery('#upload-dir').text(this.data.path);
};

httpd.Directory.prototype.contract = function() {
	var parent = this.dom.parent();
	parent.find('ul').remove();
	this.data = {
		loaded: false,
		path: this.data.path,
		contents: [],
		directories: []
	};
	jQuery('#uploaddir').text(this.data.path);
};

httpd.Directory.prototype.findDirectory = function(span) {
	for (i = 0; i < this.data.directories.length; i++) {
		if (this.data.directories[i].dom[0] == span ) {
			return this.data.directories[i];
		}
	}
};

/**
 * uploader
 */
httpd.Upload = function(dom , rootDirectory) {
	self = this;
	this.dom = dom;
	this.data = {
		directory: rootDirectory,
	};
	this.dom.click(function(){
		self.renderForm(self.data.directory);
	});
};
httpd.Upload.prototype.renderIcon = function(basedir) {
	this.data.directory = basedir;
	var output = '';
	output += '<a href="#" id="upload-button">';
	output += '<img src="/res/img/upload.png" alt="Upload files">';
	output += '<div id="upload-dir">' + basedir + '</div>';
	output += '</a>';
	this.dom.html(output);
};

httpd.Upload.prototype.renderForm = function(basedir){
	var output = '';
	output += '<div id="upload-dir"></div>';
	output += '<iframe id="upload-frame" src="about:blank"/>';
	this.dom.html(output);
	this.dom.find('#upload-dir').text(basedir);
	var frame = this.dom.find('#upload-frame');
	frame[0].src = '/res/upload.html?basedir=' + basedir;
};


