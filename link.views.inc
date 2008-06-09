<?php

/**
 * Return CCK Views data for the link_field_settings($op == 'views data').
 */
function link_views_content_field_data($field) {
  // Build the automatic views data provided for us by CCK.
  // This creates all the information necessary for the "url" field.
  $data = content_views_field_views_data($field);

  $db_info = content_database_info($field);
  $table_alias = content_views_tablename($field);
  $field_types = _content_field_types();

  // Tweak the automatic views data for the link "url" field.
  // Set the filter title to "@label URL"
  $data[$table_alias][$field['field_name'] .'_url']['filter']['title'] = t('@label URL', array('@label' => t($field_types[$field['type']]['label']))) .': '. t($field['widget']['label']);
  // Remove the argument handling for URLs.
  unset($data[$table_alias][$field['field_name'] .'_url']['argument']);

  // Build out additional views data for the link "title" field.
  $data[$table_alias][$field['field_name'] .'_title'] = array(
    'group' => t('Content'),
    'title' => t('@label title', array('@label' => t($field_types[$field['type']]['label']))) .': '. t($field['widget']['label']) . ' ('. $field['field_name'] .')',
    'help' =>  $data[$table_alias][$field['field_name'] .'_url']['help'],
    'argument' => array(
      'field' => $db_info['columns']['title']['column'],
      'tablename' => $db_info['table'],
      'handler' => 'views_handler_argument_string_content',
      'click sortable' => TRUE,
      'name field' => '', // TODO, mimic content.views.inc :)
      'content_field_name' => $field['field_name'],
      'allow_empty' => TRUE,
    ),
    'filter' => array(
      'field' => $db_info['columns']['title']['column'],
      'title' => t('@label title', array('@label' => t($field_types[$field['type']]['label']))),
      'tablename' => $db_info['table'],
      'handler' => 'views_handler_filter_string_content',
      'additional fields' => array(),
      'content_field_name' => $field['field_name'],
      'allow_empty' => TRUE,
    ),
    'sort' => array(
      'field' => $db_info['columns']['title']['column'],
      'tablename' => $db_info['table'],
      'handler' => 'views_handler_sort_content',
      'content_field_name' => $field['field_name'],
      'allow_empty' => TRUE,
    ),
  );

  // Build out additional Views filter for the link "protocol" pseudo field.
  // TODO: Add a protocol argument.
  $data[$table_alias][$field['field_name'] .'_protocol'] = array(
    'group' => t('Content'),
    'title' => t('@label protocol', array('@label' => t($field_types[$field['type']]['label']))) .': '. t($field['widget']['label']) . ' ('. $field['field_name'] .')',
    'help' =>  $data[$table_alias][$field['field_name'] .'_url']['help'],
    'filter' => array(
      'field' => $db_info['columns']['url']['column'],
      'title' => t('@label protocol', array('@label' => t($field_types[$field['type']]['label']))),
      'tablename' => $db_info['table'],
      'handler' => 'link_views_handler_filter_protocol',
      'additional fields' => array(),
      'content_field_name' => $field['field_name'],
      'allow_empty' => TRUE,
    ),
  );

  // Build out additional Views argument for the link "target" pseudo field.
  // TODO: Add a target filter.
  $data[$table_alias][$field['field_name'] .'_target'] = array(
    'group' => t('Content'),
    'title' => t('@label target', array('@label' => t($field_types[$field['type']]['label']))) .': '. t($field['widget']['label']) . ' ('. $field['field_name'] .')',
    'help' =>  $data[$table_alias][$field['field_name'] .'_url']['help'],
    'argument' => array(
      'field' => $db_info['columns']['attributes']['column'],
      'title' => t('@label target', array('@label' => t($field_types[$field['type']]['label']))) .': '. t($field['widget']['label']) . ' ('. $field['field_name'] .')',
      'tablename' => $db_info['table'],
      'handler' => 'link_views_handler_argument_target',
      'additional fields' => array(),
      'content_field_name' => $field['field_name'],
      'allow_empty' => TRUE,
    ),
  );

  return $data;
}

/**
 * Filter handler for limiting a view to URLs of a certain protocol.
 */
class link_views_handler_filter_protocol extends views_handler_filter_string {
  /**
   * Set defaults for the filter options.
   */
  function options(&$options) {
    parent::options($options);
    $options['operator'] = 'OR';
    $options['value'] = 'http';
    $options['case'] = 0;
  }

  /**
   * Define the operators supported for protocols.
   */
  function operators() {
    $operators = array(
      'OR' => array(
        'title' => t('Is one of'),
        'short' => t('='),
        'method' => 'op_protocol',
        'values' => 1,
      ),
    );

    return $operators;
  }

  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);
    $form['case'] = array(
      '#type' => 'value',
      '#value' => 0,
    );
  }

  /**
   * Provide a select list to choose the desired protocols.
   */
  function value_form(&$form, &$form_state) {
    // We have to make some choices when creating this as an exposed
    // filter form. For example, if the operator is locked and thus
    // not rendered, we can't render dependencies; instead we only
    // render the form items we need.
    $which = 'all';
    if (!empty($form_state['exposed']) && empty($this->options['expose']['operator'])) {
      $which = in_array($this->operator, $this->operator_values(1)) ? 'value' : 'none';
    }

    if ($which == 'all' || $which == 'value') {
      $form['value'] = array(
        '#type' => 'select',
        '#title' => t('Protocol'),
        '#default_value' => $this->value,
        '#options' => drupal_map_assoc(variable_get('filter_allowed_protocols', array('http', 'https', 'ftp', 'news', 'nntp', 'telnet', 'mailto', 'irc', 'ssh', 'sftp', 'webcal'))),
        '#multiple' => 1,
        '#size' => 4,
        '#description' => t('The protocols displayed here are those globally available. You may add more protocols by modifying the <em>filter_allowed_protocols</em> variable in your installation.'),
      );
    }
  }

  /**
   * Filter down the query to include only the selected protocols.
   */
  function op_protocol($field, $upper) {
    global $db_type;

    $protocols = $this->value;

    $where_conditions = array();
    foreach ($protocols as $protocol) {
      // Simple case, the URL begins with the specified protocol.
      $condition = $field . ' LIKE \''. $protocol .'%\'';

      // More complex case, no protocol specified but is automatically cleaned up
      // by link_cleanup_url(). RegEx is required for this search operation.
      if ($protocol == 'http') {
        if ($db_type == 'pgsql') {
          // PostGreSQL code has NOT been tested. Please report any problems to the link issue queue.
          // pgSQL requires all slashes to be double escaped in regular expressions.
          // See http://www.postgresql.org/docs/8.1/static/functions-matching.html#FUNCTIONS-POSIX-REGEXP
          $condition .= ' OR '. $field .' ~* \''. '^(([a-z0-9]([a-z0-9\\-_]*\\.)+)('. LINK_DOMAINS .'|[a-z][a-z]))' .'\'';
        }
        else {
          // mySQL requires backslashes to be double (triple?) escaped within character classes.
          // See http://dev.mysql.com/doc/refman/5.0/en/string-comparison-functions.html#operator_regexp
          $condition .= ' OR '. $field .' REGEXP \''. '^(([a-z0-9]([a-z0-9\\\-_]*\.)+)('. LINK_DOMAINS .'|[a-z][a-z]))' .'\'';
        }
      }

      $where_conditions[] = $condition;
    }

    $this->query->add_where($this->options['group'], implode(' '. $this->operator .' ', $where_conditions));
  }
}

/**
 * Argument handler to filter results by target.
 */
class link_views_handler_argument_target extends views_handler_argument {

  /**
   * Provide defaults for the argument when a new one is created.
   */
  function options(&$options) {
    parent::options($options);
  }

  /**
   * Provide a default options form for the argument.
   */
  function options_form(&$form, &$form_state) {
    $defaults = $this->default_actions();

    $form['title'] = array(
      '#prefix' => '<div class="clear-block">',
      '#suffix' => '</div>',
      '#type' => 'textfield',
      '#title' => t('Title'),
      '#default_value' => $this->options['title'],
      '#description' => t('The title to use when this argument is present; it will override the title of the view and titles from previous arguments. You can use percent substitution here to replace with argument titles. Use "%1" for the first argument, "%2" for the second, etc.'),
    );

    $form['clear_start'] = array(
      '#value' => '<div class="clear-block">',
    );

    $form['defaults_start'] = array(
      '#value' => '<div class="views-left-50">',
    );

    $form['default_action'] = array(
      '#type' => 'radios',
      '#title' => t('Action to take if argument is not present'),
      '#default_value' => $this->options['default_action'],
    );

    $form['defaults_stop'] = array(
      '#value' => '</div>',
    );

    $form['wildcard'] = array(
      '#prefix' => '<div class="views-right-50">',
      // prefix and no suffix means these two items will be grouped together.
      '#type' => 'textfield',
      '#title' => t('Wildcard'),
      '#size' => 20,
      '#default_value' => $this->options['wildcard'],
      '#description' => t('If this value is received as an argument, the argument will be ignored; i.e, "all values"'),
    );

    $form['wildcard_substitution'] = array(
      '#suffix' => '</div>',
      '#type' => 'textfield',
      '#title' => t('Wildcard title'),
      '#size' => 20,
      '#default_value' => $this->options['wildcard_substitution'],
      '#description' => t('The title to use for the wildcard in substitutions elsewhere.'),
    );

    $form['clear_stop'] = array(
      '#value' => '</div>',
    );

    $options = array();
    $validate_options = array();
    foreach ($defaults as $id => $info) {
      $options[$id] = $info['title'];
      if (empty($info['default only'])) {
        $validate_options[$id] = $info['title'];
      }
      if (!empty($info['form method'])) {
        $this->{$info['form method']}($form, $form_state);
      }
    }

    $form['default_action']['#options'] = $options;

    $form['validate_type'] = array(
      '#type' => 'select',
      '#title' => t('Validator'),
      '#default_value' => $this->options['validate_type'],
    );

    $validate_types = array('none' => t('<Basic validation>'));
    $plugins = views_fetch_plugin_data('argument validator');
    foreach ($plugins as $id => $info) {
      $valid = TRUE;
      if (!empty($info['type'])) {
        $valid = FALSE;
        if (empty($this->definition['validate type'])) {
          continue;
        }
        foreach ((array) $info['type'] as $type) {
          if ($type == $this->definition['validate type']) {
            $valid = TRUE;
            break;
          }
        }
      }

      // If we decide this validator is ok, add it to the list.
      if ($valid) {
        $plugin = views_get_plugin('argument validator', $id);
        if ($plugin) {
          $plugin->init($this->view, $this, $id);
          if ($plugin->access()) {
            $plugin->validate_form($form, $form_state, $id);
            $validate_types[$id] = $info['title'];
          }
        }
      }
    }

    asort($validate_types);
    $form['validate_type']['#options'] = $validate_types;
    // Show this gadget if *anything* but 'none' is selected

    $form['validate_fail'] = array(
      '#type' => 'select',
      '#title' => t('Action to take if argument does not validate'),
      '#default_value' => $this->options['validate_fail'],
      '#options' => $validate_options,
    );
  }

  /**
   * Set up the query for this argument.
   *
   * The argument sent may be found at $this->argument.
   */
  function query() {
    $this->ensure_my_table();
    // Because attributes are stored serialized, our only option is to also
    // serialize the data we're searching for and use LIKE to find similar data.
    $this->query->add_where(0, $this->table_alias .'.'. $this->real_field ." LIKE '%%%s%'", serialize(array('target' => $this->argument)));
  }
}
