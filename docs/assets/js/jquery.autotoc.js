// jQuery AutoTOC
// Dynamically create a table of contents for a page with semantically correct HTML headings.
// Version 1.0, December 30, 2016
// By Craig D. Cocca

jQuery.fn.extend({
	autoTOC: function(options) {
		
		var monitoredDOMElements = new Array();
		var windowHeight = $(window).height();
		
		// Make sure we keep tabs on the window height as the user changes the browser window dimensions
		$(window).on("resize", function() { 
			windowHeight = $(window).height();
		});
		
		// Bind monitor to scroll event to keep an eye which heading element is in the top quarter of the page
		$(window).on("scroll",function() { 
			topOfWindow = $(window).scrollTop();
			bottomOfWindow = $(window).scrollTop() + (windowHeight * .25);
			for(element in monitoredDOMElements) { 
				if(	monitoredDOMElements[element].offset >  topOfWindow && 
					monitoredDOMElements[element].offset < bottomOfWindow) { 
					$(monitoredDOMElements[element].anchor).addClass("selected");
					$(".autotoc li").not(monitoredDOMElements[element].anchor).removeClass("selected");
					break;
				}
			}
		});

		
		// Default settings
		var settings = $.extend({
			toc: "#toc"
		},options);
		
		var toc = $('<ul class="autotoc"></ul>');
		
		$(settings.toc).append(toc);
		
		return this.each(function() { 
			tocEntry = $('<li class="' + $(this).prop("tagName") + '">' + $(this).html() + '</li>');
			tocEntry.data("parentHeading",this);
			tocEntry.on("click", function() { 
				$('html, body').animate({
					scrollTop: $($(this).data("parentHeading")).offset().top - (windowHeight * .2)
    			}, 750	);	
			});
			
			$(".autotoc").append(tocEntry);
			
			monitoredDOMElements.push({
										offset: $(this).offset().top,
										anchor: tocEntry
										});
		});
	}
});